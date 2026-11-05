#pragma once

#include <QAbstractButton>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVariantAnimation>

// As peças pequenas da janela que o QSS não sabe fazer: tudo o que anima é pintado à mão.

// Um botão com os estados (rato por cima, carregado, desativado, foco) animados. Continua a ser um QPushButton:
// texto, atalhos, setDefault e os sinais de sempre.
class Botao : public QPushButton
{
    Q_OBJECT

public:
    enum class Tipo {
        Primario,   // a ação principal da janela: o único botão claro
        Secundario, // superfície elevada
        Icone,      // só um símbolo pintado, sem fundo até haver rato por cima
    };
    explicit Botao(const QString &texto, Tipo tipo = Tipo::Secundario, QWidget *pai = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void changeEvent(QEvent *) override;

private:
    void animarPara(qreal alvo);

    Tipo tipo_;
    qreal realce_ = 0; // 0 em repouso, 1 com o rato por cima
    qreal ativo_ = 1;  // 1 ativado, 0 desativado
    QVariantAnimation realceAnim_;
    QVariantAnimation ativoAnim_;
};

// Uma etiqueta de metadados: JetBrains Mono pequena, maiúsculas, espaçamento largo. Sempre texto simples.
QLabel *rotuloMeta(const QString &texto, const QColor &cor);

// O andamento da verificação: uma linha de 2 px por baixo da barra. Com total 0 não se sabe quanto falta,
// e um segmento vai e vem. Não é cyan: é a app a trabalhar, não o Claude.
class LinhaDeProgresso : public QWidget
{
    Q_OBJECT

public:
    explicit LinhaDeProgresso(QWidget *pai = nullptr);
    void definir(int feitos, int total);
    void mostrar(bool visivel);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int feitos_ = 0;
    int total_ = 0;
    qreal fase_ = 0;
    QVariantAnimation vaivem_;
};

// O estado do Claude, sempre à vista na barra: um ponto e uma etiqueta. O cyan só aparece quando o Claude
// está de facto a ler ou a responder.
class IndicadorClaude : public QWidget
{
    Q_OBJECT

public:
    enum class Estado {
        Inativo,
        APreparar, // a app está a varrer a rede e a ler os logins; o Claude ainda não entrou
        ALer,      // o pedido seguiu e o Claude está a ler a evidência
        AResponder,
        Indisponivel, // erro da API, sem chave, limite de pedidos
    };
    explicit IndicadorClaude(QWidget *pai = nullptr);

    void definir(Estado estado, const QString &texto);
    Estado estado() const { return estado_; }
    QString texto() const { return texto_; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Estado estado_ = Estado::Inativo;
    QString texto_;
    qreal pulso_ = 1;
    QVariantAnimation pulsoAnim_;
};

// Uma caixa de texto com o rebordo animado ao ganhar e perder o foco (o QSS mudaria de uma vez).
class CaixaDeTexto : public QLineEdit
{
    Q_OBJECT

public:
    explicit CaixaDeTexto(QWidget *pai = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;

private:
    void animarPara(qreal alvo);
    qreal foco_ = 0;
    QVariantAnimation anim_;
};

// Um interruptor com o texto à direita: o botão desliza em vez de saltar.
class Interruptor : public QAbstractButton
{
    Q_OBJECT

public:
    explicit Interruptor(const QString &texto, QWidget *pai = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void checkStateSet() override;
    void nextCheckState() override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    void animar();
    qreal posicao_ = 0; // 0 desligado, 1 ligado
    qreal realce_ = 0;
    QVariantAnimation anim_;
    QVariantAnimation realceAnim_;
};
