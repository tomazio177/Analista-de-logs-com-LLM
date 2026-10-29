import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { localizar } from './qt.js';

// Compila a app (app/) numa pasta de build. Usado por npm run app e npm run empacotar.
// Devolve as ferramentas encontradas e o ambiente (PATH) com as pastas do compilador e do Qt.
export function compilarApp(build, tipo = 'Release') {
  const f = localizar();
  const falta = [['Qt 6', f.qmake], ['compilador C++', f.compilador], ['CMake', f.cmake], ['Ninja', f.ninja]].filter(([, c]) => !c).map(([n]) => n);
  if (falta.length) throw new FaltaFerramenta(falta);

  // O compilador e o Qt precisam das suas pastas no PATH, tanto para compilar como para a app arrancar (as DLL do Qt).
  const pastas = [path.dirname(f.compilador), path.dirname(f.ninja), path.dirname(f.qmake), path.dirname(f.cmake)];
  const env = { ...process.env, PATH: [...pastas, process.env.PATH].join(path.delimiter) };
  const fonte = path.resolve(import.meta.dirname, '..', 'app');
  const correr = (args) => execFileSync(f.cmake, args, { stdio: 'inherit', env });
  correr([
    '-S', fonte, '-B', build, '-G', 'Ninja', `-DCMAKE_BUILD_TYPE=${tipo}`,
    `-DCMAKE_MAKE_PROGRAM=${f.ninja}`, `-DCMAKE_CXX_COMPILER=${f.compilador}`,
    ...(f.kit ? [`-DCMAKE_PREFIX_PATH=${f.kit}`] : []),
  ]);
  correr(['--build', build]);
  return { ...f, env };
}

export class FaltaFerramenta extends Error {
  constructor(falta) {
    super(`Falta instalar: ${falta.join(', ')}. Corre: npm run ferramentas`);
  }
}
