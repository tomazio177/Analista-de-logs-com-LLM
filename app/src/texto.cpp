#include "texto.h"

#include <QList>

namespace {

// Marcas que mudam a direção do texto ou escondem partes dele.
bool eMarcaDeDirecao(char32_t c)
{
    return (c >= 0x202A && c <= 0x202E)  // embeddings e overrides
        || (c >= 0x2066 && c <= 0x2069)  // isolates
        || c == 0x200E || c == 0x200F    // marcas esquerda-direita e direita-esquerda
        || c == 0x061C;                  // marca de letra árabe
}

} // namespace

QString limparTexto(const QString &texto)
{
    QString limpo;
    limpo.reserve(texto.size());
    const auto pontos = texto.toUcs4();
    for (const char32_t c : pontos) {
        const bool controlo = c < 0x20 || (c >= 0x7F && c <= 0x9F);
        if (controlo && c != '\n' && c != '\t')
            continue;
        if (eMarcaDeDirecao(c))
            continue;
        limpo.append(QString::fromUcs4(&c, 1));
    }
    return limpo;
}
