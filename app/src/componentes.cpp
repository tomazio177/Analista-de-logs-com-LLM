#include "componentes.h"

#include "tema.h"

#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>

using Tema::Cor;

namespace {

QColor misturar(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

} // namespace

// --- Botao ------------------------------------------------------------------------------------------------

Botao::Botao(const QString &texto, Tipo tipo, QWidget *pai) : QPushButton(texto, pai), tipo_(tipo)
{
    setCursor(Qt::PointingHandCursor);
    setFont(Tema::fonte(tipo == Tipo::Primario ? Tema::Letra::CorpoForte : Tema::Letra::Corpo));
    setAttribute(Qt::WA_Hover);
    for (QVariantAnimation *a : {&realceAnim_, &ativoAnim_})
        a->setEasingCurve(Tema::resposta());
    connect(&realceAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        realce_ = v.toReal();
        update();
    });
    connect(&ativoAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        ativo_ = v.toReal();
        update();
    });
    connect(this, &QPushButton::pressed, this, qOverload<>(&QWidget::update));
    connect(this, &QPushButton::released, this, qOverload<>(&QWidget::update));
}

QSize Botao::sizeHint() const
{
    if (tipo_ == Tipo::Icone)
        return {Tema::AlturaDoControlo, Tema::AlturaDoControlo};
    const int largura = fontMetrics().horizontalAdvance(text()) + (tipo_ == Tipo::Primario ? 32 : 28);
    return {largura, Tema::AlturaDoControlo};
}

void Botao::animarPara(qreal alvo)
{
    realceAnim_.stop();
    realceAnim_.setDuration(Tema::duracao(140));
    realceAnim_.setStartValue(realce_);
    realceAnim_.setEndValue(alvo);
    if (realceAnim_.duration() == 0) {
        realce_ = alvo;
        update();
        return;
    }
    realceAnim_.start();
}

void Botao::enterEvent(QEnterEvent *e)
{
    animarPara(1);
    QPushButton::enterEvent(e);
}

void Botao::leaveEvent(QEvent *e)
{
    animarPara(0);
    QPushButton::leaveEvent(e);
}

void Botao::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        ativoAnim_.stop();
        ativoAnim_.setDuration(Tema::duracao(160));
        ativoAnim_.setStartValue(ativo_);
        ativoAnim_.setEndValue(isEnabled() ? 1.0 : 0.0);
        if (ativoAnim_.duration() == 0) {
            ativo_ = isEnabled() ? 1 : 0;
            update();
        } else {
            ativoAnim_.start();
        }
    }
    QPushButton::changeEvent(e);
}

void Botao::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(0.4 + 0.6 * ativo_);
    const QRectF caixa = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool carregado = isDown();

    QColor fundo, letra, rebordo;
    switch (tipo_) {
    case Tipo::Primario:
        fundo = misturar(Tema::cor(Cor::Texto), Tema::cor(Cor::Texto).lighter(108), realce_);
        if (carregado)
            fundo = fundo.darker(112);
        letra = Tema::cor(Cor::Substrato);
        break;
    case Tipo::Secundario:
        fundo = misturar(Tema::cor(Cor::SuperficieElevada), Tema::cor(Cor::SuperficieElevada).lighter(135), realce_);
        if (carregado)
            fundo = fundo.darker(115);
        letra = Tema::cor(Cor::Texto);
        rebordo = Tema::cor(Cor::HairlineForte);
        break;
    case Tipo::Icone:
        fundo = misturar(Qt::transparent, Tema::cor(Cor::SuperficieElevada), realce_);
        letra = misturar(Tema::cor(Cor::TextoSecundario), Tema::cor(Cor::Texto), realce_);
        break;
    }

    QPainterPath forma;
    forma.addRoundedRect(caixa, Tema::Raio, Tema::Raio);
    p.fillPath(forma, fundo);
    if (rebordo.isValid())
        p.strokePath(forma, QPen(rebordo, 1));
    if (hasFocus() && focusPolicy() != Qt::NoFocus) {
        QPainterPath anel;
        anel.addRoundedRect(caixa.adjusted(-0.0, -0.0, 0, 0), Tema::Raio, Tema::Raio);
        p.strokePath(anel, QPen(Tema::cor(Cor::TextoSecundario), 1));
    }

    p.setPen(letra);
    if (tipo_ == Tipo::Icone) {
        // As definições: três traços com um nó cada, desenhados ao pixel lógico (não é um ícone de sistema).
        p.setPen(QPen(letra, 1.2, Qt::SolidLine, Qt::RoundCap));
        const QPointF c = caixa.center();
        const qreal largura = 12;
        const qreal nos[] = {-3, 3, -1};
        for (int i = 0; i < 3; ++i) {
            const qreal y = c.y() - 4 + i * 4;
            p.drawLine(QPointF(c.x() - largura / 2, y), QPointF(c.x() + largura / 2, y));
            p.setBrush(fundo.alpha() ? fundo : Tema::cor(Cor::Superficie));
            p.drawEllipse(QPointF(c.x() + nos[i], y), 1.8, 1.8);
        }
        return;
    }
    p.setFont(font());
    p.drawText(rect(), Qt::AlignCenter, text());
}

// --- Etiqueta de metadados --------------------------------------------------------------------------------

QLabel *rotuloMeta(const QString &texto, const QColor &cor)
{
    auto *l = new QLabel(texto.toUpper());
    l->setTextFormat(Qt::PlainText);
    l->setFont(Tema::fonte(Tema::Letra::Meta));
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, cor);
    l->setPalette(p);
    return l;
}

// --- IndicadorClaude --------------------------------------------------------------------------------------

IndicadorClaude::IndicadorClaude(QWidget *pai) : QWidget(pai)
{
    setFont(Tema::fonte(Tema::Letra::Meta));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    pulsoAnim_.setStartValue(1.0);
    pulsoAnim_.setKeyValueAt(0.5, 0.35);
    pulsoAnim_.setEndValue(1.0);
    pulsoAnim_.setEasingCurve(Tema::ciclo());
    pulsoAnim_.setLoopCount(-1);
    connect(&pulsoAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        pulso_ = v.toReal();
        update(QRect(0, 0, 14, height())); // só o ponto e o halo
    });
}

void IndicadorClaude::definir(Estado estado, const QString &texto)
{
    estado_ = estado;
    texto_ = texto.toUpper();
    setAccessibleName(texto);
    const bool pulsa = (estado == Estado::APreparar || estado == Estado::ALer) && !Tema::reduzirMovimento();
    pulsoAnim_.stop();
    pulso_ = 1;
    if (pulsa) {
        pulsoAnim_.setDuration(estado == Estado::ALer ? 1400 : 2200);
        pulsoAnim_.start();
    }
    updateGeometry();
    update();
}

QSize IndicadorClaude::sizeHint() const
{
    return {20 + fontMetrics().horizontalAdvance(texto_), Tema::AlturaDoControlo};
}

void IndicadorClaude::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor ponto, letra = Tema::cor(Cor::TextoSecundario);
    switch (estado_) {
    case Estado::Inativo: ponto = Tema::cor(Cor::TextoTenue); break;
    case Estado::APreparar: ponto = Tema::cor(Cor::TextoSecundario); break;
    case Estado::ALer:
    case Estado::AResponder:
        ponto = Tema::cor(Cor::Claude);
        letra = Tema::cor(Cor::Claude);
        break;
    case Estado::Indisponivel:
        ponto = Tema::cor(Cor::Aviso); // algo precisa de ti; o vermelho é só da gravidade
        letra = Tema::cor(Cor::Texto);
        break;
    }
    const QPointF centro(6.5, height() / 2.0);
    if (estado_ == Estado::ALer || estado_ == Estado::AResponder) {
        // Um halo mínimo: o ponto do Claude é luz, os outros são tinta.
        QColor halo = ponto;
        halo.setAlphaF(0.22 * pulso_);
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(centro, 6, 6);
    }
    ponto.setAlphaF(ponto.alphaF() * (0.35 + 0.65 * pulso_));
    p.setPen(Qt::NoPen);
    p.setBrush(ponto);
    p.drawEllipse(centro, 3, 3);
    p.setPen(letra);
    p.setFont(font());
    p.drawText(QRect(20, 0, width() - 20, height()), Qt::AlignVCenter | Qt::AlignLeft, texto_);
}

// --- LinhaDeProgresso -------------------------------------------------------------------------------------

LinhaDeProgresso::LinhaDeProgresso(QWidget *pai) : QWidget(pai)
{
    setFixedHeight(2);
    vaivem_.setStartValue(0.0);
    vaivem_.setEndValue(1.0);
    vaivem_.setDuration(1600);
    vaivem_.setEasingCurve(Tema::ciclo());
    vaivem_.setLoopCount(-1);
    connect(&vaivem_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        fase_ = v.toReal();
        update();
    });
    hide();
}

void LinhaDeProgresso::definir(int feitos, int total)
{
    feitos_ = feitos;
    total_ = total;
    const bool vaiEVem = total <= 0 && isVisible() && !Tema::reduzirMovimento();
    if (vaiEVem && vaivem_.state() != QAbstractAnimation::Running)
        vaivem_.start();
    else if (!vaiEVem)
        vaivem_.stop();
    update();
}

void LinhaDeProgresso::mostrar(bool visivel)
{
    setVisible(visivel);
    definir(visivel ? feitos_ : 0, visivel ? total_ : 0);
}

void LinhaDeProgresso::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Tema::cor(Cor::Hairline));
    const QColor cor = Tema::cor(Cor::TextoSecundario);
    if (total_ > 0) {
        p.fillRect(QRectF(0, 0, width() * qBound(0.0, double(feitos_) / total_, 1.0), height()), cor);
        return;
    }
    if (Tema::reduzirMovimento()) {
        // Sem movimento, "a trabalhar" diz-se com a linha inteira, mais ténue.
        QColor tenue = cor;
        tenue.setAlphaF(0.45);
        p.fillRect(rect(), tenue);
        return;
    }
    const qreal segmento = width() * 0.22;
    const qreal x = -segmento + (width() + segmento) * fase_;
    QLinearGradient g(x, 0, x + segmento, 0);
    QColor transparente = cor;
    transparente.setAlphaF(0);
    g.setColorAt(0, transparente);
    g.setColorAt(0.5, cor);
    g.setColorAt(1, transparente);
    p.fillRect(QRectF(x, 0, segmento, height()), g);
}

// --- CaixaDeTexto ------------------------------------------------------------------------------------------------

CaixaDeTexto::CaixaDeTexto(QWidget *pai) : QLineEdit(pai)
{
    // O rebordo é pintado aqui; o do QSS fica transparente para manter o mesmo espaço interior.
    setStyleSheet(QStringLiteral("QLineEdit { border-color: transparent; }"));
    anim_.setEasingCurve(Tema::resposta());
    connect(&anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        foco_ = v.toReal();
        update();
    });
}

void CaixaDeTexto::animarPara(qreal alvo)
{
    anim_.stop();
    anim_.setDuration(Tema::duracao(140));
    anim_.setStartValue(foco_);
    anim_.setEndValue(alvo);
    if (anim_.duration() == 0) {
        foco_ = alvo;
        update();
    } else {
        anim_.start();
    }
}

void CaixaDeTexto::focusInEvent(QFocusEvent *e)
{
    animarPara(1);
    QLineEdit::focusInEvent(e);
}

void CaixaDeTexto::focusOutEvent(QFocusEvent *e)
{
    animarPara(0);
    QLineEdit::focusOutEvent(e);
}

void CaixaDeTexto::paintEvent(QPaintEvent *e)
{
    QLineEdit::paintEvent(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath forma;
    forma.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Tema::Raio, Tema::Raio);
    QColor cor = misturar(Tema::cor(Cor::HairlineForte), Tema::cor(Cor::TextoSecundario), foco_);
    if (!isEnabled())
        cor = Tema::cor(Cor::Hairline);
    p.strokePath(forma, QPen(cor, 1));
}

// --- Interruptor ------------------------------------------------------------------------------------------

Interruptor::Interruptor(const QString &texto, QWidget *pai) : QAbstractButton(pai)
{
    setText(texto);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFont(Tema::fonte(Tema::Letra::Corpo));
    setAttribute(Qt::WA_Hover);
    for (QVariantAnimation *a : {&anim_, &realceAnim_})
        a->setEasingCurve(Tema::resposta());
    connect(&anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        posicao_ = v.toReal();
        update();
    });
    connect(&realceAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        realce_ = v.toReal();
        update();
    });
}

QSize Interruptor::sizeHint() const
{
    return {32 + 10 + fontMetrics().horizontalAdvance(text()) + 4, 24};
}

void Interruptor::checkStateSet()
{
    animar();
}

void Interruptor::nextCheckState()
{
    QAbstractButton::nextCheckState();
    animar();
}

void Interruptor::animar()
{
    anim_.stop();
    anim_.setDuration(Tema::duracao(160));
    anim_.setStartValue(posicao_);
    anim_.setEndValue(isChecked() ? 1.0 : 0.0);
    if (anim_.duration() == 0 || !isVisible()) {
        posicao_ = isChecked() ? 1 : 0;
        update();
    } else {
        anim_.start();
    }
}

void Interruptor::enterEvent(QEnterEvent *e)
{
    realceAnim_.stop();
    realceAnim_.setDuration(Tema::duracao(140));
    realceAnim_.setStartValue(realce_);
    realceAnim_.setEndValue(1.0);
    realceAnim_.start();
    QAbstractButton::enterEvent(e);
}

void Interruptor::leaveEvent(QEvent *e)
{
    realceAnim_.stop();
    realceAnim_.setDuration(Tema::duracao(140));
    realceAnim_.setStartValue(realce_);
    realceAnim_.setEndValue(0.0);
    realceAnim_.start();
    QAbstractButton::leaveEvent(e);
}

void Interruptor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF trilho(0.5, (height() - 18) / 2.0 + 0.5, 32, 17);
    QPainterPath forma;
    forma.addRoundedRect(trilho, 8.5, 8.5);
    const QColor desligado = misturar(Tema::cor(Cor::Substrato), Tema::cor(Cor::SuperficieElevada), realce_);
    p.fillPath(forma, misturar(desligado, Tema::cor(Cor::Texto), posicao_));
    p.strokePath(forma, QPen(hasFocus() ? Tema::cor(Cor::TextoSecundario) : Tema::cor(Cor::HairlineForte), 1));
    const qreal x = trilho.left() + 3 + (trilho.width() - 17) * posicao_;
    p.setPen(Qt::NoPen);
    p.setBrush(misturar(Tema::cor(Cor::TextoSecundario), Tema::cor(Cor::Substrato), posicao_));
    p.drawEllipse(QRectF(x, trilho.top() + 2.5, 12, 12));
    p.setPen(Tema::cor(Cor::Texto));
    p.setFont(font());
    p.drawText(QRectF(42, 0, width() - 42, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}
