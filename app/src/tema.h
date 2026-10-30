#pragma once

#include <QColor>
#include <QEasingCurve>
#include <QFont>
#include <QString>

class QApplication;

// O sistema visual da app, num só sítio: cores, letras, medidas e movimento. As regras estão em DESIGN.md.
// Nenhum outro ficheiro escreve uma cor em hexadecimal nem escolhe uma fonte.
namespace Tema {

enum class Cor {
    Substrato,         // o fundo da janela e da evidência
    Superficie,        // barra e painel de análise
    SuperficieElevada, // botões, campos, faixas, linha selecionada
    Hairline,          // a separação entre compartimentos
    HairlineForte,     // rebordo dos campos e anel do foco
    Texto,
    TextoSecundario,
    TextoTenue,        // números de linha: não é texto para ler
    Claude,            // só a voz do Claude: o feixe, as citações, o estado dele
    ClaudeTenue,       // fundo dos chips e halo das linhas citadas
    Erro,              // só gravidade alta e falhas que impedem a triagem
    Aviso,             // gravidade média, nível "atenção"
};
QColor cor(Cor c);
// Para o QSS: "rgba(r,g,b,a)", que serve também às cores com transparência.
QString css(Cor c);

enum class Letra {
    Numeral,    // Geist 700, o número grande
    Destaque,   // Geist 700, a frase grande de um estado sem número (vazio, a ler)
    Titulo,     // Geist 700, título de um achado
    Corpo,      // Geist 400
    CorpoForte, // Geist 700, ao tamanho do corpo
    Log,        // JetBrains Mono 400, a evidência
    Meta,       // JetBrains Mono 400, pequena, para maiúsculas com espaçamento largo
};
QFont fonte(Letra l);

// A gravidade de um achado ("alta", "media", "baixa", "info") ou de um nível geral ("urgente", "atencao", "tranquilo").
// Só alta/urgente e média/atenção têm cor; o resto fica no texto secundário.
QColor daGravidade(const QString &gravidade);

constexpr int Raio = 4;
constexpr int AlturaDaLinha = 20; // uma linha de evidência
constexpr int AlturaDoControlo = 30;

// Movimento. Com "Reduzir movimento", as durações passam a 0 (os chamadores mostram o estado final de uma vez).
bool reduzirMovimento();
void definirReduzirMovimento(bool reduzir);
int duracao(int ms);
inline QEasingCurve resposta() { return QEasingCurve::OutCubic; } // a uma ação de quem usa
inline QEasingCurve chegada() { return QEasingCurve::OutExpo; }   // algo que chega
inline QEasingCurve ciclo() { return QEasingCurve::InOutSine; }   // só a passagem do feixe

// Carrega as fontes embebidas, a paleta e o QSS. Chamar logo depois de criar a QApplication.
void aplicar(QApplication &app);

} // namespace Tema
