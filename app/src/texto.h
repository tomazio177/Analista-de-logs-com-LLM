#pragma once

#include <QString>

// Os nomes dos aparelhos e dos utilizadores vêm da rede, tal como chegaram: qualquer pessoa pode dar ao telemóvel
// um nome com caracteres de controlo, ou com um que inverte o sentido do texto (U+202E) e faz "exe.txt" parecer "txt.exe".
// Tudo o que vem do motor passa por aqui antes de ser mostrado.
QString limparTexto(const QString &texto);
