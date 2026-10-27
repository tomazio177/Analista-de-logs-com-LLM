import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const correr = promisify(execFile);

// Código de saída que o script usa quando o Windows recusa o acesso ao registo de Segurança.
export const SEM_PERMISSAO = 5;

// Lê os eventos 4625 (login falhado) do registo de Segurança através do PowerShell.
// O Get-WinEvent dá os campos do evento por posição; o XML dá-os pelo nome, que não muda entre versões do Windows.
const script = (dias) => `
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
# Sem privilégios, o -FilterHashtable não diz "acesso negado": diz que não encontrou eventos, e o resultado
# saía como 0 logins falhados. O -LogName dá o erro certo, por isso o acesso confirma-se primeiro com ele.
try { $null = Get-WinEvent -LogName Security -MaxEvents 1 } catch {
  if ($_.Exception -is [System.UnauthorizedAccessException]) { exit ${SEM_PERMISSAO} }
}
try {
  $eventos = Get-WinEvent -FilterHashtable @{ LogName = 'Security'; Id = 4625; StartTime = (Get-Date).AddDays(-${dias}) } -MaxEvents 5000
} catch {
  if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { '[]'; exit 0 }
  if ($_.Exception -is [System.UnauthorizedAccessException] -or $_.FullyQualifiedErrorId -like '*UnauthorizedAccess*') { exit ${SEM_PERMISSAO} }
  [Console]::Error.WriteLine($_.Exception.Message); exit 1
}
$lista = foreach ($e in $eventos) {
  $campos = @{}
  foreach ($d in ([xml]$e.ToXml()).Event.EventData.Data) { $campos[$d.Name] = $d.'#text' }
  [pscustomobject]@{ id = $e.RecordId; quando = $e.TimeCreated.ToUniversalTime().ToString('o'); campos = $campos }
}
ConvertTo-Json -InputObject @($lista) -Depth 3 -Compress
`;

export async function lerLoginsFalhados(dias) {
  // -EncodedCommand evita todos os problemas de aspas: o PowerShell recebe o script em UTF-16 codificado em base64.
  const codificado = Buffer.from(script(dias), 'utf16le').toString('base64');
  const { stdout } = await correr(
    'powershell.exe',
    ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', codificado],
    { windowsHide: true, maxBuffer: 64 * 1024 * 1024 },
  );
  return JSON.parse(stdout.trim() || '[]');
}
