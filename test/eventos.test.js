import { test } from 'node:test';
import assert from 'node:assert/strict';
import { lerEventosXml, agruparLoginsFalhados } from '../src/eventos.js';

// Formato do `wevtutil qe Security /f:xml`: eventos colados, sem elemento raiz.
const evento = ({ data, utilizador, ip, tipo, status = '0xc000006d', subStatus = '0xc000006a', estacao = '-' }) =>
  `<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Microsoft-Windows-Security-Auditing' Guid='{54849625-5478-4994-a5ba-3e3b0328c30d}'/><EventID>4625</EventID><Version>0</Version><Level>0</Level><Task>12544</Task><Opcode>0</Opcode><Keywords>0x8010000000000000</Keywords><TimeCreated SystemTime='${data}'/><EventRecordID>1</EventRecordID><Correlation/><Execution ProcessID='812' ThreadID='900'/><Channel>Security</Channel><Computer>DESKTOP-CASA</Computer><Security/></System><EventData><Data Name='SubjectUserSid'>S-1-0-0</Data><Data Name='SubjectUserName'>-</Data><Data Name='TargetUserSid'>S-1-0-0</Data><Data Name='TargetUserName'>${utilizador}</Data><Data Name='TargetDomainName'></Data><Data Name='Status'>${status}</Data><Data Name='FailureReason'>%%2313</Data><Data Name='SubStatus'>${subStatus}</Data><Data Name='LogonType'>${tipo}</Data><Data Name='LogonProcessName'>NtLmSsp </Data><Data Name='WorkstationName'>${estacao}</Data><Data Name='TransmittedServices'>-</Data><Data Name='KeyLength'>0</Data><Data Name='ProcessId'>0x0</Data><Data Name='ProcessName'>-</Data><Data Name='IpAddress'>${ip}</Data><Data Name='IpPort'>51234</Data></EventData></Event>`;

test('lê os campos que interessam de cada evento 4625', () => {
  const xml = evento({ data: '2026-10-20T21:14:03.1234567Z', utilizador: 'admin', ip: '192.168.1.50', tipo: 3, estacao: 'KALI' });
  assert.deepEqual(lerEventosXml(xml), [
    { data: '2026-10-20T21:14:03.1234567Z', utilizador: 'admin', origem: '192.168.1.50', tipoLogin: 3, motivo: 'password errada', estacao: 'KALI' },
  ]);
});

test('o motivo vem do SubStatus quando existe, senão do Status', () => {
  const [inexistente, bloqueada] = lerEventosXml(
    evento({ data: '2026-10-20T10:00:00Z', utilizador: 'x', ip: '-', tipo: 2, subStatus: '0xC0000064' }) +
      evento({ data: '2026-10-20T10:00:01Z', utilizador: 'x', ip: '-', tipo: 2, status: '0xc0000234', subStatus: '0x0' }),
  );
  assert.equal(inexistente.motivo, 'utilizador não existe');
  assert.equal(bloqueada.motivo, 'conta bloqueada');
});

test('origens locais, IPv4 mapeado em IPv6 e campos vazios ficam normalizados', () => {
  const eventos = lerEventosXml(
    evento({ data: 'a', utilizador: '-', ip: '127.0.0.1', tipo: 3 }) +
      evento({ data: 'b', utilizador: 'joao', ip: '::ffff:192.168.1.7', tipo: 3 }) +
      evento({ data: 'c', utilizador: 'joao', ip: '-', tipo: 2 }),
  );
  assert.equal(eventos[0].origem, 'local');
  assert.equal(eventos[0].utilizador, null);
  assert.equal(eventos[0].estacao, null);
  assert.equal(eventos[1].origem, '192.168.1.7');
  assert.equal(eventos[2].origem, 'local'); // login ao teclado sem IP: é mesmo local
});

test('um login de rede sem IP fica com origem desconhecida, não local', () => {
  // RDP com NLA e NTLM por SMB registam o 4625 com LogonType 3 e IpAddress "-".
  const [e] = lerEventosXml(evento({ data: 'a', utilizador: 'admin', ip: '-', tipo: 3, estacao: 'KALI' }));
  assert.equal(e.origem, 'desconhecida');
});

test('descodifica entidades XML e corta caracteres de controlo vindos de quem ataca', () => {
  const [e] = lerEventosXml(evento({ data: 'a', utilizador: 'Jo&#227;o &amp; &lt;b&gt;&#27;[2J', ip: '-', tipo: 2 }));
  assert.equal(e.utilizador, 'João & <b> [2J');
});

test('aceita <Data/> vazio e ignora texto que não seja um evento', () => {
  const xml = `lixo antes <Event xmlns='x'><System><TimeCreated SystemTime='t'/></System><EventData><Data Name='IpAddress'/><Data Name='LogonType'>10</Data></EventData></Event>`;
  const [e] = lerEventosXml(xml);
  assert.equal(e.origem, 'desconhecida');
  assert.equal(e.tipoLogin, 10);
});

test('agrupa por origem e tipo: força bruta e "password spraying" ficam visíveis como tal', () => {
  const eventos = [
    ...Array.from({ length: 30 }, (_, i) => ({ data: `2026-10-20T21:${String(i).padStart(2, '0')}:00Z`, utilizador: 'admin', origem: '192.168.1.50', tipoLogin: 3, motivo: 'password errada', estacao: 'KALI' })),
    ...['ana', 'rui', 'teste', 'root', 'user', 'pi'].map((u) => ({ data: '2026-10-20T22:00:00Z', utilizador: u, origem: '192.168.1.60', tipoLogin: 3, motivo: 'utilizador não existe', estacao: null })),
    { data: '2026-10-20T08:00:00Z', utilizador: 'tomas', origem: 'local', tipoLogin: 2, motivo: 'password errada', estacao: null },
  ];
  const [forcaBruta, spraying, local] = agruparLoginsFalhados(eventos);

  assert.equal(forcaBruta.id, 'l1');
  assert.equal(forcaBruta.tentativas, 30);
  assert.equal(forcaBruta.primeira, '2026-10-20T21:00:00Z');
  assert.equal(forcaBruta.ultima, '2026-10-20T21:29:00Z');
  assert.deepEqual(forcaBruta.utilizadores, [{ valor: 'admin', tentativas: 30 }]);
  assert.deepEqual(forcaBruta.estacoes, ['KALI']);

  assert.equal(spraying.tentativas, 6);
  assert.equal(spraying.utilizadoresDistintos, 6);
  assert.equal(spraying.utilizadores.length, 5); // só os 5 mais frequentes seguem para o modelo

  assert.equal(local.origem, 'local');
  assert.equal(local.tipoLogin, 2);
});
