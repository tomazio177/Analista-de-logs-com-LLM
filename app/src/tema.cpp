#include "tema.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Tema {

namespace {

const QString Geist = QStringLiteral("Geist");
const QString Mono = QStringLiteral("JetBrains Mono");

// As fontes vão dentro do executável (licença OFL, em licencas/). Carregadas uma vez, à primeira letra pedida:
// os testes criam janelas sem passar por aplicar().
void carregarFontes()
{
    static const bool carregadas = [] {
        for (const char *f : {":/fontes/Geist-Regular.ttf", ":/fontes/Geist-Bold.ttf", ":/fontes/JetBrainsMono-Regular.ttf"})
            QFontDatabase::addApplicationFont(QString::fromLatin1(f));
        return true;
    }();
    Q_UNUSED(carregadas);
}

QFont letra(const QString &familia, int pixeis, int peso)
{
    carregarFontes();
    QFont f(familia);
    f.setFamilies({familia}); // sem recair numa fonte do sistema
    f.setPixelSize(pixeis);
    f.setWeight(static_cast<QFont::Weight>(peso));
    // O mesmo desenho a 100% e a 150%: sem o hinting a puxar as letras para a grelha de pixéis.
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

bool animacoesDoWindows()
{
#ifdef Q_OS_WIN
    BOOL ligadas = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &ligadas, 0))
        return ligadas != FALSE;
#endif
    return true;
}

const char *const ChaveMovimento = "reduzirMovimento";

} // namespace

QColor cor(Cor c)
{
    switch (c) {
    case Cor::Substrato: return QColor(0x07, 0x0B, 0x14);
    case Cor::Superficie: return QColor(0x0C, 0x12, 0x20);
    case Cor::SuperficieElevada: return QColor(0x11, 0x1A, 0x2C);
    case Cor::Hairline: return QColor(255, 255, 255, 15);      // 0,06
    case Cor::HairlineForte: return QColor(255, 255, 255, 26); // 0,10
    case Cor::Texto: return QColor(0xE6, 0xED, 0xF7);
    case Cor::TextoSecundario: return QColor(0x7D, 0x8A, 0xA0);
    case Cor::TextoTenue: return QColor(0x6E, 0x7A, 0x8E); // 4,5:1 no substrato: os números de linha também são texto
    case Cor::Claude: return QColor(0x3D, 0xE0, 0xFF);
    case Cor::ClaudeTenue: return QColor(0x3D, 0xE0, 0xFF, 36); // 0,14
    case Cor::Erro: return QColor(0xFF, 0x4D, 0x4D);
    case Cor::Aviso: return QColor(0xFF, 0xB0, 0x20);
    }
    return {};
}

QString css(Cor c)
{
    const QColor k = cor(c);
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(k.red()).arg(k.green()).arg(k.blue()).arg(k.alpha());
}

QFont fonte(Letra l)
{
    switch (l) {
    case Letra::Numeral: {
        QFont f = letra(Geist, 56, QFont::Bold);
        f.setLetterSpacing(QFont::PercentageSpacing, 98);
        return f;
    }
    case Letra::Destaque: return letra(Geist, 26, QFont::Bold);
    case Letra::Titulo: return letra(Geist, 15, QFont::Bold);
    case Letra::Corpo: return letra(Geist, 13, QFont::Normal);
    case Letra::CorpoForte: return letra(Geist, 13, QFont::Bold);
    case Letra::Log: return letra(Mono, 12, QFont::Normal);
    case Letra::Meta: {
        QFont f = letra(Mono, 10, QFont::Normal);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        return f;
    }
    }
    return {};
}

QColor daGravidade(const QString &g)
{
    if (g == QLatin1String("alta") || g == QLatin1String("urgente"))
        return cor(Cor::Erro);
    if (g == QLatin1String("media") || g == QLatin1String("atencao"))
        return cor(Cor::Aviso);
    return cor(Cor::TextoSecundario);
}

bool reduzirMovimento()
{
    const QSettings s;
    return s.contains(QLatin1String(ChaveMovimento)) ? s.value(QLatin1String(ChaveMovimento)).toBool() : !animacoesDoWindows();
}

void definirReduzirMovimento(bool reduzir)
{
    QSettings().setValue(QLatin1String(ChaveMovimento), reduzir);
}

int duracao(int ms)
{
    return reduzirMovimento() ? 0 : ms;
}

void aplicar(QApplication &app)
{
    carregarFontes();
    app.setFont(fonte(Letra::Corpo));
    // A barra de título do Windows e os diálogos do sistema também escuros, mesmo com o Windows em modo claro.
    // Só existe a partir do Qt 6.8; com um Qt mais antigo a barra segue o Windows.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif

    QPalette p;
    p.setColor(QPalette::Window, cor(Cor::Substrato));
    p.setColor(QPalette::Base, cor(Cor::Substrato));
    p.setColor(QPalette::AlternateBase, cor(Cor::Superficie));
    p.setColor(QPalette::Button, cor(Cor::SuperficieElevada));
    p.setColor(QPalette::WindowText, cor(Cor::Texto));
    p.setColor(QPalette::Text, cor(Cor::Texto));
    p.setColor(QPalette::ButtonText, cor(Cor::Texto));
    p.setColor(QPalette::BrightText, cor(Cor::Texto));
    p.setColor(QPalette::PlaceholderText, cor(Cor::TextoSecundario));
    p.setColor(QPalette::ToolTipBase, cor(Cor::SuperficieElevada));
    p.setColor(QPalette::ToolTipText, cor(Cor::Texto));
    p.setColor(QPalette::Highlight, cor(Cor::SuperficieElevada).lighter(160));
    p.setColor(QPalette::HighlightedText, cor(Cor::Texto));
    p.setColor(QPalette::Link, cor(Cor::Texto));
    p.setColor(QPalette::Mid, cor(Cor::HairlineForte));
    for (const auto papel : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, papel, cor(Cor::TextoTenue));
    app.setPalette(p);

    // O QSS só trata do que o Qt desenha por nós. O que anima ou tem profundidade é pintado à mão (componentes.cpp,
    // evidencia.cpp), porque o QSS não tem transições.
    const QString t = QStringLiteral(R"(
        QToolTip { background: %3; color: %5; border: 1px solid %4; padding: 4px 6px; }
        QLineEdit { background: %1; color: %5; border: 1px solid %6; border-radius: %8px; padding: 0 8px;
                    min-height: %9px; selection-background-color: %3; }
        QLineEdit:focus { border-color: %10; }
        QLineEdit:disabled { color: %7; }
        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: %4; border-radius: 3px; min-height: 32px; margin: 2px; }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QSplitter::handle { background: %4; }
        QDialog, QMessageBox { background: %2; }
        QCheckBox { color: %5; spacing: 8px; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %6; border-radius: 3px; background: %1; }
        QCheckBox::indicator:checked { background: %5; border-color: %5; }
    )")
                          .arg(css(Cor::Substrato), css(Cor::Superficie), css(Cor::SuperficieElevada), css(Cor::Hairline), css(Cor::Texto),
                               css(Cor::HairlineForte), css(Cor::TextoTenue))
                          .arg(Raio)
                          .arg(AlturaDoControlo - 2)
                          .arg(css(Cor::TextoSecundario));
    app.setStyleSheet(t);
}

} // namespace Tema
