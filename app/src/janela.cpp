#include "janela.h"

#include "administrador.h"
#include "chave.h"
#include "componentes.h"
#include "evidencia.h"
#include "motor.h"
#include "tema.h"
#include "texto.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QEnterEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QSplitter>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

using Tema::Cor;
using Tema::Letra;

namespace {

// Tudo o que vem do motor chega como texto simples: um QLabel interpreta HTML por defeito, e um aparelho chamado
// "<img src=...>" seria desenhado em vez de lido. Por isso o formato é sempre PlainText, e o texto passa por limparTexto.
QLabel *etiqueta(const QString &texto, Letra letra = Letra::Corpo, Cor cor = Cor::Texto)
{
    auto *l = new QLabel(limparTexto(texto));
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setFont(Tema::fonte(letra));
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, Tema::cor(cor));
    l->setPalette(p);
    return l;
}

void pintar(QLabel *l, const QColor &cor)
{
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, cor);
    l->setPalette(p);
}

void esvaziar(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        } else if (QLayout *l = item->layout()) {
            esvaziar(l);
        }
        delete item;
    }
}

QString nomeDaGravidade(const QString &g)
{
    if (g == QLatin1String("alta")) return QStringLiteral("ALTA");
    if (g == QLatin1String("media")) return QStringLiteral("MÉDIA");
    if (g == QLatin1String("baixa")) return QStringLiteral("BAIXA");
    return QStringLiteral("INFO");
}

int ordemDaGravidade(const QString &g)
{
    static const QStringList ordem{QStringLiteral("alta"), QStringLiteral("media"), QStringLiteral("baixa"), QStringLiteral("info")};
    const int i = ordem.indexOf(g);
    return i < 0 ? ordem.size() : i;
}

QString horaLocal(const QString &iso)
{
    const QDateTime t = QDateTime::fromString(iso, Qt::ISODate);
    return t.isValid() ? QLocale().toString(t.toLocalTime(), QStringLiteral("dd/MM HH:mm")) : QString();
}

QStringList lista(const QJsonValue &v)
{
    QStringList r;
    for (const QJsonValue &x : v.toArray())
        r << x.toString();
    return r;
}

QFrame *hairline(Qt::Orientation o = Qt::Horizontal)
{
    auto *f = new QFrame;
    if (o == Qt::Horizontal)
        f->setFixedHeight(1);
    else
        f->setFixedWidth(1);
    f->setStyleSheet(QStringLiteral("background: %1;").arg(Tema::css(Cor::Hairline)));
    return f;
}

// Um compartimento com uma régua vertical de 2 px à esquerda: a cor diz a gravidade, ou que é a voz do Claude.
class Regua : public QWidget
{
public:
    explicit Regua(const QColor &cor, QWidget *pai = nullptr) : QWidget(pai), cor_(cor) {}
    void definirCor(const QColor &c)
    {
        cor_ = c;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(0, 0, 2, height()), cor_);
    }

private:
    QColor cor_;
};

// A barra superior: superfície, com uma hairline em baixo.
class Barra : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Tema::cor(Cor::Superficie));
        p.fillRect(QRect(0, height() - 1, width(), 1), Tema::cor(Cor::Hairline));
    }
};

// O painel de análise: superfície, separado da evidência por uma hairline (a do divisor).
class Superficie : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Tema::cor(Cor::Superficie));
    }
};

} // namespace

JanelaPrincipal::JanelaPrincipal(Motor *motor, QWidget *pai) : QMainWindow(pai), motor_(motor)
{
    setWindowTitle(QStringLiteral("Analista de logs"));
    resize(1280, 800);
    setMinimumSize(900, 600);

    auto *centro = new QWidget;
    auto *raiz = new QVBoxLayout(centro);
    raiz->setContentsMargins(0, 0, 0, 0);
    raiz->setSpacing(0);

    // --- A barra: o nome, o contexto, o estado do Claude e a ação.
    auto *barra = new Barra;
    barra->setFixedHeight(48);
    auto *b = new QHBoxLayout(barra);
    b->setContentsMargins(16, 0, 12, 0);
    b->setSpacing(16);
    b->addWidget(rotuloMeta(QStringLiteral("Analista de logs"), Tema::cor(Cor::Texto)));
    contexto_ = rotuloMeta(QString(), Tema::cor(Cor::TextoSecundario));
    b->addWidget(contexto_);
    estado_ = etiqueta(QString(), Letra::Corpo, Cor::TextoSecundario);
    estado_->setWordWrap(false);
    estado_->hide();
    b->addWidget(estado_, 1);
    b->addStretch(0);
    indicador_ = new IndicadorClaude;
    indicador_->definir(IndicadorClaude::Estado::Inativo, QStringLiteral("Claude · inativo"));
    b->addWidget(indicador_);
    auto *definicoes = new Botao(QString(), Botao::Tipo::Icone);
    definicoes->setToolTip(QStringLiteral("Definições"));
    definicoes->setAccessibleName(QStringLiteral("Definições"));
    connect(definicoes, &QPushButton::clicked, this, &JanelaPrincipal::abrirDefinicoes);
    b->addWidget(definicoes);
    botao_ = new Botao(QStringLiteral("Verificar agora"), Botao::Tipo::Primario);
    botao_->setDefault(true);
    botao_->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    botao_->setToolTip(QStringLiteral("Varre a rede, lê os logins falhados e pede a triagem (Ctrl+R)"));
    b->addWidget(botao_);
    raiz->addWidget(barra);

    progresso_ = new LinhaDeProgresso;
    raiz->addWidget(progresso_);

    auto *containerFaixas = new QWidget;
    faixas_ = new QVBoxLayout(containerFaixas);
    faixas_->setContentsMargins(0, 0, 0, 0);
    faixas_->setSpacing(0);
    raiz->addWidget(containerFaixas);

    // --- Os dois painéis.
    auto *divisor = new QSplitter(Qt::Horizontal);
    divisor->setHandleWidth(1);
    divisor->setChildrenCollapsible(false);
    evidencia_ = new PainelEvidencia;
    evidencia_->setMinimumWidth(360);
    divisor->addWidget(evidencia_);

    auto *analise = new Superficie;
    analise->setMinimumWidth(420);
    auto *a = new QVBoxLayout(analise);
    a->setContentsMargins(0, 0, 0, 0);
    auto *conteudo = new QWidget;
    auto *c = new QVBoxLayout(conteudo);
    c->setContentsMargins(28, 28, 28, 28);
    c->setSpacing(0);

    nivel_ = rotuloMeta(QStringLiteral("—"), Tema::cor(Cor::TextoSecundario));
    c->addWidget(nivel_);
    c->addSpacing(8);
    auto *linhaNumeral = new QHBoxLayout;
    linhaNumeral->setSpacing(16);
    numeral_ = new QLabel(QStringLiteral("—"));
    numeral_->setFont(Tema::fonte(Letra::Numeral));
    numeral_->setTextFormat(Qt::PlainText);
    linhaNumeral->addWidget(numeral_, 0, Qt::AlignBottom);
    legenda_ = etiqueta(QString(), Letra::Corpo, Cor::TextoSecundario);
    legenda_->setContentsMargins(0, 0, 0, 9); // assenta na linha de base do numeral
    linhaNumeral->addWidget(legenda_, 1, Qt::AlignBottom);
    c->addLayout(linhaNumeral);
    chamada_ = etiqueta(QString(), Letra::Destaque);
    chamada_->hide();
    c->addWidget(chamada_);
    c->addSpacing(14);
    meta_ = rotuloMeta(QString(), Tema::cor(Cor::TextoSecundario));
    c->addWidget(meta_);
    c->addSpacing(20);

    // O resumo é a voz do Claude: a régua cyan à esquerda.
    resumoCaixa_ = new Regua(Tema::cor(Cor::Claude));
    auto *r = new QVBoxLayout(resumoCaixa_);
    r->setContentsMargins(14, 0, 0, 0);
    resumo_ = etiqueta(QString());
    r->addWidget(resumo_);
    c->addWidget(resumoCaixa_);

    passos_ = new QWidget;
    auto *p = new QVBoxLayout(passos_);
    p->setContentsMargins(0, 0, 0, 0);
    p->setSpacing(10);
    const QStringList textos{QStringLiteral("Varre a rede: que aparelhos estão ligados e que portas têm abertas."),
                             QStringLiteral("Lê os logins falhados deste PC (o Windows pede autorização)."),
                             QStringLiteral("O Claude lê tudo, explica o que merece atenção e mostra em que linhas o viu.")};
    for (int i = 0; i < textos.size(); ++i) {
        auto *linha = new QHBoxLayout;
        linha->setSpacing(14);
        auto *n = rotuloMeta(QStringLiteral("0%1").arg(i + 1), Tema::cor(Cor::TextoTenue));
        linha->addWidget(n, 0, Qt::AlignTop);
        linha->addWidget(etiqueta(textos[i], Letra::Corpo, Cor::TextoSecundario), 1);
        p->addLayout(linha);
    }
    passos_->hide();
    c->addSpacing(4);
    c->addWidget(passos_);

    avisos_ = new QVBoxLayout;
    avisos_->setSpacing(6);
    avisos_->setContentsMargins(0, 16, 0, 0);
    c->addLayout(avisos_);
    achados_ = new QVBoxLayout;
    achados_->setSpacing(0);
    achados_->setContentsMargins(0, 20, 0, 0);
    c->addLayout(achados_);
    tituloPerguntas_ = rotuloMeta(QStringLiteral("Perguntas que só tu sabes responder"), Tema::cor(Cor::TextoSecundario));
    tituloPerguntas_->setContentsMargins(0, 28, 0, 12);
    tituloPerguntas_->hide();
    c->addWidget(tituloPerguntas_);
    perguntas_ = new QVBoxLayout;
    perguntas_->setSpacing(18);
    c->addLayout(perguntas_);
    c->addStretch(1);

    rolo_ = new QScrollArea;
    rolo_->setWidgetResizable(true);
    rolo_->setFrameShape(QFrame::NoFrame);
    rolo_->setWidget(conteudo);
    a->addWidget(rolo_);
    divisor->addWidget(analise);
    divisor->setStretchFactor(0, 44);
    divisor->setStretchFactor(1, 56);
    divisor->setSizes({560, 720});
    raiz->addWidget(divisor, 1);
    setCentralWidget(centro);

    revelacao_.setEasingCurve(Tema::resposta());

    connect(botao_, &QPushButton::clicked, this, [this] { verificarAgora(); });
    connect(motor_, &Motor::terminou, this, &JanelaPrincipal::aoTerminar);
    connect(motor_, &Motor::falhou, this, &JanelaPrincipal::aoFalhar);
    connect(motor_, &Motor::progresso, this, &JanelaPrincipal::aoProgredir);
    connect(motor_, &Motor::terminouComoAdministrador, this, &JanelaPrincipal::aoTerminarComoAdministrador);

    if (!motor_->localizacao().ok()) {
        botao_->setEnabled(false);
        faixa(motor_->localizacao().erro, Faixa::Erro);
    }
    estadoVazio(QStringLiteral("A abrir…"));
}

// --- Pedidos ao motor -------------------------------------------------------------------------------------

void JanelaPrincipal::arrancar()
{
    carregarUltima();
    motor_->executar(QStringLiteral("config.js"));
}

void JanelaPrincipal::carregarUltima()
{
    motor_->executar(QStringLiteral("triar.js"), {QStringLiteral("--ultima")});
}

void JanelaPrincipal::verificarAgora(bool aprovar)
{
    if (ocupado_)
        return;
    limparFaixas();
    definirOcupado(true);
    aprovar_ = aprovar;
    if (processoElevado()) {
        correrVigia(false);
        return;
    }
    // No Windows, sem administrador: os logins leem-se primeiro, num processo à parte com autorização do Windows,
    // e a vigia corre depois sem esse passo. A janela nunca tem mais poderes do que precisa.
    definirEstado(QStringLiteral("A pedir ao Windows autorização para ler os logins…"));
    QStringList args{QStringLiteral("--json")};
    if (!pastaDeDados_.isEmpty())
        args << QStringLiteral("--dados=") + pastaDeDados_;
    motor_->executarComoAdministrador(QStringLiteral("logs.js"), args);
}

void JanelaPrincipal::aoTerminarComoAdministrador(const QString &script, int codigo)
{
    if (script != QLatin1String("logs.js"))
        return;
    if (codigo == -1)
        faixa(QStringLiteral("Sem autorização do Windows, os logins não foram lidos. A verificação continua sem eles."), Faixa::Aviso);
    else if (codigo != 0)
        faixa(QStringLiteral("Não consegui ler os logins do Windows (código %1). A verificação continua sem eles.").arg(codigo), Faixa::Aviso);
    correrVigia(true);
}

void JanelaPrincipal::correrVigia(bool semLogs)
{
    definirEstado(QStringLiteral("A verificar…"));
    QStringList args;
    if (aprovar_)
        args << QStringLiteral("--aprovar");
    if (semLogs)
        args << QStringLiteral("--sem-logs");
    motor_->executar(QStringLiteral("vigiar.js"), args);
}

void JanelaPrincipal::pedirChave(bool primeiraVez)
{
    DialogoChave dialogo(motor_, primeiraVez, this);
    if (dialogo.exec() != QDialog::Accepted)
        return;
    faixa(dialogo.aviso().isEmpty() ? QStringLiteral("Chave guardada.") : QStringLiteral("Chave guardada. ") + dialogo.aviso(), Faixa::Info);
}

void JanelaPrincipal::abrirDefinicoes()
{
    QDialog d(this);
    d.setWindowTitle(QStringLiteral("Definições"));
    auto *v = new QVBoxLayout(&d);
    v->setContentsMargins(24, 22, 24, 20);
    v->setSpacing(10);
    v->addWidget(rotuloMeta(QStringLiteral("Movimento"), Tema::cor(Cor::TextoSecundario)));
    auto *movimento = new QCheckBox(QStringLiteral("Reduzir movimento"));
    movimento->setChecked(Tema::reduzirMovimento());
    v->addWidget(movimento);
    v->addWidget(etiqueta(QStringLiteral("Sem o feixe a varrer a evidência nem animações: as marcas e os resultados aparecem de uma vez."),
                          Letra::Corpo, Cor::TextoSecundario));
    v->addSpacing(12);
    v->addWidget(rotuloMeta(QStringLiteral("Claude"), Tema::cor(Cor::TextoSecundario)));
    auto *chave = new Botao(QStringLiteral("Chave da API…"));
    v->addWidget(chave, 0, Qt::AlignLeft);
    v->addSpacing(16);
    auto *fechar = new Botao(QStringLiteral("Fechar"), Botao::Tipo::Primario);
    fechar->setDefault(true);
    v->addWidget(fechar, 0, Qt::AlignRight);
    connect(fechar, &QPushButton::clicked, &d, &QDialog::accept);
    connect(chave, &QPushButton::clicked, &d, [this, &d] {
        d.accept();
        QTimer::singleShot(0, this, [this] { pedirChave(false); });
    });
    connect(movimento, &QCheckBox::toggled, this, [](bool r) { Tema::definirReduzirMovimento(r); });
    d.setMinimumWidth(380);
    d.exec();
}

void JanelaPrincipal::responder(int numero, const QString &texto)
{
    // "--texto=…" numa só peça: uma resposta que comece por "-" não é tomada por outra opção.
    motor_->executar(QStringLiteral("notas.js"), {QStringLiteral("--pergunta"), QString::number(numero), QStringLiteral("--texto=") + texto});
}

void JanelaPrincipal::pedirAprovacao(const QJsonObject &router)
{
    const QString quem = router.isEmpty() ? QString()
                                          : QStringLiteral("Router: %1 (%2)\n\n").arg(limparTexto(router.value(QStringLiteral("ip")).toString()),
                                                                                    limparTexto(router.value(QStringLiteral("mac")).toString()));
    QMessageBox caixa(this);
    caixa.setIcon(QMessageBox::Question);
    caixa.setWindowTitle(QStringLiteral("Aprovar esta rede?"));
    caixa.setText(QStringLiteral("Aprovar esta rede?"));
    caixa.setInformativeText(quem
                             + QStringLiteral("Só aprovas se a rede for tua, ou se quem manda nela te deu autorização. "
                                              "Varrer redes de outras pessoas é crime em Portugal (Lei 109/2009)."));
    auto *aprovar = caixa.addButton(QStringLiteral("Aprovar e verificar"), QMessageBox::AcceptRole);
    caixa.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    caixa.setDefaultButton(qobject_cast<QPushButton *>(caixa.buttons().last()));
    caixa.exec();
    if (caixa.clickedButton() == aprovar)
        verificarAgora(true);
}

// --- Respostas do motor -----------------------------------------------------------------------------------

void JanelaPrincipal::aoTerminar(const QString &script, const QJsonObject &r)
{
    if (script == QLatin1String("vigiar.js")) {
        definirOcupado(false);
        mostrarVigia(r);
    } else if (script == QLatin1String("triar.js")) {
        if (r.value(QStringLiteral("ok")).toBool())
            mostrarTriagem(r);
        else if (r.value(QStringLiteral("erro")).toObject().value(QStringLiteral("codigo")).toString() == QLatin1String("sem-triagem"))
            primeiroArranque();
        else
            faixa(r.value(QStringLiteral("erro")).toObject().value(QStringLiteral("mensagem")).toString(), Faixa::Erro);
        emit carregou();
    } else if (script == QLatin1String("config.js") && r.contains(QStringLiteral("chave_definida"))) {
        // Ao abrir: guarda a pasta de dados (o processo elevado tem de a receber) e, sem chave, pede-a já.
        pastaDeDados_ = r.value(QStringLiteral("pasta_de_dados")).toString();
        if (!r.value(QStringLiteral("chave_definida")).toBool())
            QTimer::singleShot(0, this, [this] { pedirChave(true); });
    } else if (script == QLatin1String("notas.js")) {
        if (r.value(QStringLiteral("ok")).toBool())
            carregarUltima();
        else
            faixa(r.value(QStringLiteral("erro")).toObject().value(QStringLiteral("mensagem")).toString(), Faixa::Erro);
    }
}

void JanelaPrincipal::aoFalhar(const QString &script, const QString &mensagem)
{
    if (script == QLatin1String("vigiar.js")) {
        definirOcupado(false);
        if (evidencia_->aLer())
            evidencia_->terminarLeitura({});
        indicador_->definir(IndicadorClaude::Estado::Indisponivel, QStringLiteral("Claude · sem resposta"));
    }
    faixa(mensagem, Faixa::Erro);
    if (script == QLatin1String("triar.js")) {
        estadoVazio(QString());
        emit carregou();
    }
}

void JanelaPrincipal::aoProgredir(const QString &script, const QJsonObject &e)
{
    if (script != QLatin1String("vigiar.js"))
        return;
    const QString fase = e.value(QStringLiteral("fase")).toString();
    if (fase == QLatin1String("inicio")) {
        definirEstado(limparTexto(QStringLiteral("A verificar a rede %1…").arg(e.value(QStringLiteral("rede")).toString())));
        indicador_->definir(IndicadorClaude::Estado::APreparar, QStringLiteral("A varrer a rede"));
    } else if (fase == QLatin1String("descobrir")) {
        const int feitos = e.value(QStringLiteral("feitos")).toInt(), total = e.value(QStringLiteral("total")).toInt();
        progresso_->definir(feitos, total);
        definirEstado(QStringLiteral("A procurar aparelhos… %1/%2").arg(feitos).arg(total));
        indicador_->definir(IndicadorClaude::Estado::APreparar, QStringLiteral("A varrer a rede · %1/%2").arg(feitos).arg(total));
    } else if (fase == QLatin1String("portas")) {
        progresso_->definir(0, 0); // daqui em diante o motor não diz quanto falta
        definirEstado(QStringLiteral("A testar as portas de %1 aparelhos. Depois lê os logins e pede a triagem, o que pode demorar um minuto.")
                          .arg(e.value(QStringLiteral("aparelhos")).toInt()));
        indicador_->definir(IndicadorClaude::Estado::APreparar, QStringLiteral("A testar portas"));
    } else if (fase == QLatin1String("triagem")) {
        // O pedido vai seguir: a evidência passa a ser a que o Claude está a ler, e o feixe varre-a.
        evidencia_->definirDados(e.value(QStringLiteral("dados")).toObject(), QStringLiteral("agora"));
        estadoVazio(QStringLiteral("O Claude está a ler %1 linhas.").arg(evidencia_->total()));
        pintar(chamada_, Tema::cor(Cor::Texto));
        nivel_->setText(QStringLiteral("A LER"));
        pintar(nivel_, Tema::cor(Cor::Claude));
        meta_->setText(QStringLiteral("OS ACHADOS APARECEM AQUI, CADA UM COM AS LINHAS EM QUE SE APOIA"));
        definirEstado(QStringLiteral("O Claude está a ler. Costuma demorar um minuto."));
        indicador_->definir(IndicadorClaude::Estado::ALer, QStringLiteral("Claude a ler · %1 linhas").arg(evidencia_->total()));
        evidencia_->comecarLeitura();
    }
}

void JanelaPrincipal::erroDaTriagem(const QJsonObject &triagem)
{
    // Uma frase clara e o que fazer, decididos pelo código. Nunca o texto técnico da API à frente.
    const QJsonObject erro = triagem.value(QStringLiteral("erro")).toObject();
    const QString codigo = erro.value(QStringLiteral("codigo")).toString();
    const QString mensagem = erro.value(QStringLiteral("mensagem")).toString();
    const auto chave = [this] { pedirChave(false); };
    const auto outraVez = [this] { verificarAgora(aprovar_); };
    QString estado = QStringLiteral("Claude indisponível");
    if (codigo == QLatin1String("sem-chave")) {
        faixa(QStringLiteral("Falta a chave da API para pedir a triagem."), Faixa::Erro, QStringLiteral("Guardar a chave…"), chave);
        estado = QStringLiteral("Claude · falta a chave");
    } else if (codigo == QLatin1String("chave-recusada")) {
        faixa(QStringLiteral("A Anthropic recusou a chave da API. Pode ter sido revogada ou mal copiada."), Faixa::Erro, QStringLiteral("Trocar a chave…"), chave);
        estado = QStringLiteral("Claude · chave recusada");
    } else if (codigo == QLatin1String("limite-de-pedidos")) {
        faixa(QStringLiteral("Atingiste o limite de pedidos à API. Espera um minuto e tenta outra vez."), Faixa::Aviso, QStringLiteral("Tentar outra vez"), outraVez);
        estado = QStringLiteral("Claude · limite atingido");
    } else if (codigo == QLatin1String("sem-ligacao")) {
        faixa(QStringLiteral("Não consegui chegar ao Claude. Confirma a ligação à internet."), Faixa::Erro, QStringLiteral("Tentar outra vez"), outraVez);
        estado = QStringLiteral("Claude · sem ligação");
    } else if (codigo == QLatin1String("resposta-cortada") || codigo == QLatin1String("resposta-invalida")) {
        faixa(QStringLiteral("A resposta do Claude chegou incompleta ou fora do formato, por isso não a mostro. Ficou guardada; tenta outra vez."),
              Faixa::Aviso, QStringLiteral("Tentar outra vez"), outraVez);
    } else if (codigo == QLatin1String("recusa")) {
        faixa(QStringLiteral("O Claude recusou fazer esta triagem. Ficou registado; os dados continuam à esquerda."), Faixa::Aviso);
        estado = QStringLiteral("Claude · recusou");
    } else if (codigo == QLatin1String("sem-varrimentos")) {
        faixa(QStringLiteral("Ainda não há nenhum varrimento da rede para o Claude ler."), Faixa::Aviso);
        estado = QStringLiteral("Claude · inativo");
    } else {
        QString texto = mensagem;
        const QString detalhe = triagem.value(QStringLiteral("detalhe")).toString();
        if (!detalhe.isEmpty())
            texto += QStringLiteral("\n") + detalhe;
        faixa(texto, Faixa::Erro);
    }
    indicador_->definir(codigo == QLatin1String("sem-varrimentos") ? IndicadorClaude::Estado::Inativo : IndicadorClaude::Estado::Indisponivel, estado);
}

void JanelaPrincipal::mostrarVigia(const QJsonObject &r)
{
    const auto parte = [&r](const char *nome) { return r.value(QLatin1String(nome)).toObject(); };
    const auto ok = [](const QJsonObject &o) { return o.value(QStringLiteral("ok")).toBool(); };
    const auto codigo = [](const QJsonObject &o) { return o.value(QStringLiteral("erro")).toObject().value(QStringLiteral("codigo")).toString(); };
    const auto mensagem = [](const QJsonObject &o) { return o.value(QStringLiteral("erro")).toObject().value(QStringLiteral("mensagem")).toString(); };

    QStringList resumo;
    const QJsonObject varrimento = parte("varrimento");
    if (ok(varrimento)) {
        resumo << QStringLiteral("%1 aparelhos").arg(varrimento.value(QStringLiteral("aparelhos")).toArray().size());
        resumo << QStringLiteral("%1 mudanças").arg(varrimento.value(QStringLiteral("mudancas")).toArray().size());
    } else if (codigo(varrimento) == QLatin1String("rede-por-aprovar")) {
        const QJsonObject router = varrimento.value(QStringLiteral("router")).toObject();
        faixa(QStringLiteral("Esta rede ainda não foi aprovada, por isso não varri nada."), Faixa::Aviso, QStringLiteral("Aprovar e verificar…"),
              [this, router] { pedirAprovacao(router); });
    } else {
        faixa(mensagem(varrimento), Faixa::Aviso);
    }

    const QJsonObject logins = parte("logins");
    if (r.value(QStringLiteral("logins")).isNull()) {
        // Os logins foram lidos à parte, como administrador (ou a pessoa não deu autorização: já há uma faixa a dizê-lo).
    } else if (ok(logins)) {
        resumo << QStringLiteral("%1 logins novos").arg(logins.value(QStringLiteral("novos")).toInt());
        if (!logins.value(QStringLiteral("periodos_possivelmente_incompletos")).toArray().isEmpty())
            faixa(QStringLiteral("O Windows já apagou alguns logins falhados antigos antes de serem lidos. Os que aparecem são reais, mas pode faltar algum."), Faixa::Info);
    } else if (codigo(logins) == QLatin1String("sem-permissao")) {
        faixa(QStringLiteral("Os logins falhados do Windows precisam de administrador. Fecha a app e abre-a com botão direito → «Executar como administrador»."), Faixa::Aviso);
    } else if (codigo(logins) != QLatin1String("nao-windows")) {
        faixa(mensagem(logins), Faixa::Aviso);
    }

    const QJsonObject triagem = parte("triagem");
    const auto mostrar = [this, triagem, ok] {
        if (ok(triagem))
            mostrarTriagem(triagem);
        else
            erroDaTriagem(triagem);
    };
    // Se o feixe ainda está a varrer, faz a última passagem e só então chegam os resultados.
    if (evidencia_->aLer())
        evidencia_->terminarLeitura(mostrar);
    else
        mostrar();
    definirEstado(resumo.isEmpty() ? QString()
                                   : QStringLiteral("Verificado às %1: %2.").arg(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat), resumo.join(QStringLiteral(", "))));
}

// --- O que se vê ------------------------------------------------------------------------------------------

void JanelaPrincipal::definirOcupado(bool ocupado)
{
    ocupado_ = ocupado;
    botao_->setEnabled(!ocupado && motor_->localizacao().ok());
    botao_->setText(ocupado ? QStringLiteral("A verificar…") : QStringLiteral("Verificar agora"));
    botao_->updateGeometry();
    progresso_->mostrar(ocupado);
    if (ocupado) {
        progresso_->definir(0, 0);
        indicador_->definir(IndicadorClaude::Estado::APreparar, QStringLiteral("A preparar"));
    }
}

void JanelaPrincipal::definirEstado(const QString &texto)
{
    estado_->setText(limparTexto(texto));
    estado_->setVisible(!texto.isEmpty());
}

void JanelaPrincipal::limparFaixas()
{
    esvaziar(faixas_);
}

void JanelaPrincipal::faixa(const QString &texto, Faixa tipo, const QString &botao, const std::function<void()> &acao)
{
    // Inline, por baixo da barra: régua de 2 px na cor da gravidade e o texto. Nunca um diálogo para um erro.
    auto *f = new QFrame;
    f->setObjectName(QStringLiteral("faixa"));
    f->setAutoFillBackground(true);
    QPalette pal = f->palette();
    pal.setColor(QPalette::Window, Tema::cor(Cor::SuperficieElevada));
    f->setPalette(pal);
    auto *linha = new QHBoxLayout(f);
    linha->setContentsMargins(16, 10, 12, 10);
    linha->setSpacing(14);
    const QColor cor = tipo == Faixa::Erro ? Tema::cor(Cor::Erro) : tipo == Faixa::Aviso ? Tema::cor(Cor::Aviso) : Tema::cor(Cor::TextoSecundario);
    auto *regua = new Regua(cor);
    regua->setFixedWidth(2);
    linha->addWidget(regua);
    linha->addWidget(etiqueta(texto), 1);
    if (!botao.isEmpty()) {
        auto *b = new Botao(botao);
        connect(b, &QPushButton::clicked, this, [acao] { acao(); });
        linha->addWidget(b, 0, Qt::AlignVCenter);
    }
    faixas_->addWidget(f);
    faixas_->addWidget(hairline());

    // Entra a abrir em altura, sem saltar.
    if (const int ms = Tema::duracao(160)) {
        auto *anim = new QVariantAnimation(f);
        anim->setStartValue(0);
        anim->setEndValue(f->sizeHint().height());
        anim->setDuration(ms);
        anim->setEasingCurve(Tema::resposta());
        connect(anim, &QVariantAnimation::valueChanged, f, [f](const QVariant &h) { f->setMaximumHeight(h.toInt()); });
        connect(anim, &QVariantAnimation::finished, f, [f] { f->setMaximumHeight(QWIDGETSIZE_MAX); });
        f->setMaximumHeight(0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void JanelaPrincipal::estadoVazio(const QString &texto)
{
    // Sem número: o cabeçalho é uma frase ("A abrir…"), sem a régua cyan, que é só da voz do Claude.
    nivel_->setText(QString());
    pintar(nivel_, Tema::cor(Cor::TextoSecundario));
    numeral_->hide();
    legenda_->hide();
    chamada_->setText(texto);
    pintar(chamada_, Tema::cor(Cor::TextoSecundario));
    chamada_->setVisible(!texto.isEmpty());
    meta_->setText(QString());
    resumo_->setText(QString());
    resumoCaixa_->setVisible(false);
    passos_->hide();
    contexto_->setText(QString());
    esvaziar(avisos_);
    esvaziar(achados_);
    esvaziar(perguntas_);
    tituloPerguntas_->hide();
    porRevelar_.clear();
    revelacao_.stop();
}

void JanelaPrincipal::primeiroArranque()
{
    // Sem triagem nenhuma: a evidência mostra um exemplo esbatido e a análise ensina o único caminho.
    estadoVazio(QStringLiteral("Ainda não verificaste esta rede."));
    pintar(chamada_, Tema::cor(Cor::Texto));
    nivel_->setText(QStringLiteral("AINDA SEM TRIAGEM"));
    meta_->setText(QStringLiteral("CARREGA EM «VERIFICAR AGORA» · CTRL+R"));
    passos_->show();
    evidencia_->mostrarExemplo();
    indicador_->definir(IndicadorClaude::Estado::Inativo, QStringLiteral("Claude · à espera"));
}

void JanelaPrincipal::mostrarTriagem(const QJsonObject &t)
{
    estadoVazio(QString());

    // A evidência: o que seguiu para o Claude, e as linhas que cada achado cita.
    const QString quando = horaLocal(t.value(QStringLiteral("data")).toString());
    if (t.contains(QStringLiteral("dados")))
        evidencia_->definirDados(t.value(QStringLiteral("dados")).toObject(), quando.isEmpty() ? QString() : QStringLiteral("a ") + quando);
    else if (evidencia_->exemplo())
        evidencia_->limpar();

    QList<QJsonObject> achados;
    for (const QJsonValue &a : t.value(QStringLiteral("achados")).toArray())
        achados << a.toObject();
    std::stable_sort(achados.begin(), achados.end(), [](const QJsonObject &x, const QJsonObject &y) {
        return ordemDaGravidade(x.value(QStringLiteral("gravidade")).toString()) < ordemDaGravidade(y.value(QStringLiteral("gravidade")).toString());
    });

    // O cabeçalho respira à volta de um número: os achados que pedem atenção (gravidade média ou alta).
    const QString nivel = t.value(QStringLiteral("nivel_geral")).toString();
    const bool tranquilo = nivel == QLatin1String("tranquilo");
    nivel_->setText(tranquilo ? QStringLiteral("TRANQUILO") : nivel == QLatin1String("urgente") ? QStringLiteral("URGENTE") : QStringLiteral("ATENÇÃO"));
    pintar(nivel_, Tema::daGravidade(nivel));
    const int graves = int(std::count_if(achados.begin(), achados.end(), [](const QJsonObject &a) {
        return ordemDaGravidade(a.value(QStringLiteral("gravidade")).toString()) <= 1;
    }));
    chamada_->hide();
    numeral_->show();
    legenda_->show();
    numeral_->setText(QString::number(graves));
    pintar(numeral_, graves ? Tema::cor(Cor::Texto) : Tema::cor(Cor::TextoSecundario));
    legenda_->setText(graves == 1 ? QStringLiteral("achado pede atenção")
                      : graves    ? QStringLiteral("achados pedem atenção")
                      : achados.isEmpty() ? QStringLiteral("nada a assinalar")
                                          : QStringLiteral("achados, nenhum grave"));

    QStringList meta;
    if (!quando.isEmpty())
        meta << QStringLiteral("Triagem %1").arg(quando);
    meta << (achados.size() == 1 ? QStringLiteral("1 achado") : QStringLiteral("%1 achados").arg(achados.size()));
    const int nPerguntas = int(t.value(QStringLiteral("perguntas")).toArray().size());
    if (nPerguntas)
        meta << (nPerguntas == 1 ? QStringLiteral("1 pergunta") : QStringLiteral("%1 perguntas").arg(nPerguntas));
    meta_->setText(meta.join(QStringLiteral(" · ")).toUpper());
    contexto_->setText(t.value(QStringLiteral("dados")).toObject().value(QStringLiteral("rede")).toString().isEmpty()
                           ? QString()
                           : QStringLiteral("REDE ") + limparTexto(t.value(QStringLiteral("dados")).toObject().value(QStringLiteral("rede")).toString()));

    resumo_->setText(limparTexto(t.value(QStringLiteral("resumo")).toString()));
    resumoCaixa_->setVisible(!resumo_->text().isEmpty());

    for (const QString &aviso : lista(t.value(QStringLiteral("avisos")))) {
        auto *l = etiqueta(QStringLiteral("⚠ ") + aviso, Letra::Corpo, Cor::Aviso);
        avisos_->addWidget(l);
    }

    // As citações de cada achado e, nas linhas citadas, a gravidade do achado (para a minimapa).
    QVector<int> todas;
    QHash<int, int> gravidades;
    for (const QJsonObject &a : achados) {
        const QVector<int> citadas = citacoesDoAchado(a, evidencia_->linhas());
        const int g = ordemDaGravidade(a.value(QStringLiteral("gravidade")).toString());
        for (int i : citadas) {
            if (!todas.contains(i))
                todas << i;
            if (g <= 1)
                gravidades[i] = std::max(gravidades.value(i), g == 0 ? 2 : 1);
        }
        auto *cartao = cartaoDoAchado(a, citadas);
        achados_->addWidget(cartao);
        porRevelar_ << cartao;
    }
    evidencia_->marcar(todas, gravidades);

    const QJsonArray perguntas = t.value(QStringLiteral("perguntas")).toArray();
    tituloPerguntas_->setVisible(!perguntas.isEmpty());
    for (const QJsonValue &p : perguntas)
        perguntas_->addWidget(cartaoDaPergunta(p.toObject()));

    indicador_->definir(IndicadorClaude::Estado::Inativo, quando.isEmpty() ? QStringLiteral("Claude · inativo") : QStringLiteral("Claude · triagem de ") + quando);
    revelar();
}

void JanelaPrincipal::revelar()
{
    // Os achados entram um a um: é a apresentação do texto que chegou inteiro, não streaming.
    // Com Reduzir movimento, ou nos testes sem janela à vista, aparece tudo de uma vez.
    if (Tema::reduzirMovimento() || !isVisible() || porRevelar_.isEmpty())
        return;
    const int passo = 90, cada = 220;
    const int total = int(porRevelar_.size()) * passo + cada;
    for (QWidget *w : porRevelar_)
        w->setProperty("revelado", 0.0);
    const QString estadoFinal = indicador_->texto(); // o que fica quando a revelação acaba
    indicador_->definir(IndicadorClaude::Estado::AResponder, QStringLiteral("Claude a responder"));
    revelacao_.stop();
    disconnect(&revelacao_, nullptr, this, nullptr);
    revelacao_.setStartValue(0.0);
    revelacao_.setEndValue(qreal(total));
    revelacao_.setDuration(total);
    revelacao_.setEasingCurve(QEasingCurve::Linear); // é o relógio; cada achado tem a sua curva abaixo
    connect(&revelacao_, &QVariantAnimation::valueChanged, this, [this, passo, cada](const QVariant &v) {
        const qreal agora = v.toReal();
        for (int i = 0; i < porRevelar_.size(); ++i) {
            const qreal t = qBound(0.0, (agora - i * passo) / cada, 1.0);
            porRevelar_[i]->setProperty("revelado", QEasingCurve(QEasingCurve::OutCubic).valueForProgress(t));
            porRevelar_[i]->update();
        }
    });
    connect(&revelacao_, &QVariantAnimation::finished, this, [this, estadoFinal] {
        for (QWidget *w : porRevelar_)
            w->setProperty("revelado", 1.0);
        porRevelar_.clear();
        // Um erro que chegue entretanto (outra verificação) fica: não se sobrepõe com "inativo".
        if (indicador_->estado() == IndicadorClaude::Estado::AResponder)
            indicador_->definir(IndicadorClaude::Estado::Inativo, estadoFinal);
    });
    revelacao_.start();
}

namespace {

// Um achado: régua de gravidade à esquerda, sem caixa. Entra com fade e uma subida de 8 px (propriedade "revelado").
class Achado : public QWidget
{
public:
    Achado(const QColor &gravidade) : gravidade_(gravidade) { setProperty("revelado", 1.0); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(0, 20, 2, height() - 40), gravidade_);
        p.fillRect(QRect(0, height() - 1, width(), 1), Tema::cor(Cor::Hairline));
    }
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::DynamicPropertyChange) {
            const qreal r = property("revelado").toReal();
            if (!efeito_ && r < 1) {
                efeito_ = new QGraphicsOpacityEffect(this);
                setGraphicsEffect(efeito_);
            }
            if (efeito_) {
                efeito_->setOpacity(r);
                setContentsMargins(0, int((1 - r) * 8), 0, 0);
                if (r >= 1) {
                    setGraphicsEffect(nullptr); // sem efeito parado por cima de um widget que faz scroll
                    efeito_ = nullptr;
                    setContentsMargins(0, 0, 0, 0);
                }
            }
        }
        return QWidget::event(e);
    }

private:
    QColor gravidade_;
    QGraphicsOpacityEffect *efeito_ = nullptr;
};

// Um chip de citação: "L014" ou "L015–L018", cyan sobre cyan ténue. Tab chega lá, Enter ativa.
class Chip : public QPushButton
{
public:
    Chip(const QString &texto) : QPushButton(texto)
    {
        setFont(Tema::fonte(Letra::Meta));
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setToolTip(QStringLiteral("Mostrar na evidência"));
        realce_.setDuration(Tema::duracao(140));
        realce_.setEasingCurve(Tema::resposta());
        connect(&realce_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            r_ = v.toReal();
            update();
        });
    }
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 14, 20}; }

protected:
    void enterEvent(QEnterEvent *e) override
    {
        animar(1);
        QPushButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override
    {
        animar(0);
        QPushButton::leaveEvent(e);
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor fundo = Tema::cor(Cor::ClaudeTenue);
        fundo.setAlphaF(fundo.alphaF() * (1 + 1.2 * r_));
        QPainterPath forma;
        forma.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Tema::Raio, Tema::Raio);
        p.fillPath(forma, fundo);
        if (hasFocus())
            p.strokePath(forma, QPen(Tema::cor(Cor::Claude), 1));
        p.setPen(Tema::cor(Cor::Claude));
        p.setFont(font());
        p.drawText(rect(), Qt::AlignCenter, text());
    }

private:
    void animar(qreal alvo)
    {
        realce_.stop();
        realce_.setStartValue(r_);
        realce_.setEndValue(alvo);
        if (realce_.duration() == 0) {
            r_ = alvo;
            update();
        } else {
            realce_.start();
        }
    }
    QVariantAnimation realce_;
    qreal r_ = 0;
};

} // namespace

QWidget *JanelaPrincipal::cartaoDoAchado(const QJsonObject &a, const QVector<int> &citadas)
{
    const QString gravidade = a.value(QStringLiteral("gravidade")).toString();
    auto *c = new Achado(Tema::daGravidade(gravidade));
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(18, 20, 0, 20);
    v->setSpacing(6);

    v->addWidget(rotuloMeta(nomeDaGravidade(gravidade), Tema::daGravidade(gravidade)));
    v->addWidget(etiqueta(a.value(QStringLiteral("titulo")).toString(), Letra::Titulo));

    if (a.contains(QStringLiteral("gravidade_do_modelo")))
        v->addWidget(etiqueta(QStringLiteral("O modelo deu %1; num alerta vindo da rede, as regras não deixam baixar de %2.")
                                  .arg(nomeDaGravidade(a.value(QStringLiteral("gravidade_do_modelo")).toString()).toLower(), nomeDaGravidade(gravidade).toLower()),
                              Letra::Corpo, Cor::Aviso));

    const auto seccao = [v](const QString &titulo, const QStringList &linhas, bool voz = false) {
        if (linhas.isEmpty())
            return;
        v->addSpacing(8);
        v->addWidget(rotuloMeta(titulo, Tema::cor(Cor::TextoSecundario)));
        QWidget *alvo = nullptr;
        QVBoxLayout *destino = v;
        if (voz) {
            // A conclusão é a voz do Claude: régua cyan.
            alvo = new Regua(Tema::cor(Cor::Claude));
            destino = new QVBoxLayout(alvo);
            destino->setContentsMargins(12, 0, 0, 0);
            destino->setSpacing(2);
        }
        for (const QString &l : linhas)
            destino->addWidget(etiqueta(linhas.size() > 1 ? QStringLiteral("·  ") + l : l, Letra::Corpo, Cor::Texto));
        if (alvo)
            v->addWidget(alvo);
    };
    seccao(QStringLiteral("Nos dados"), lista(a.value(QStringLiteral("nos_dados"))));
    seccao(QStringLiteral("Conhecimento geral (não vem dos teus dados)"), lista(a.value(QStringLiteral("conhecimento_geral"))));
    seccao(QStringLiteral("Conclusão"), {a.value(QStringLiteral("conclusao")).toString()}, true);
    seccao(QStringLiteral("O que fazer"), {a.value(QStringLiteral("acao_sugerida")).toString()});

    const QStringList semApoio = lista(a.value(QStringLiteral("factos_sem_apoio")));
    if (!semApoio.isEmpty())
        v->addWidget(etiqueta(QStringLiteral("⚠ Citado como facto, mas não está nos dados enviados: ") + semApoio.join(QStringLiteral(", ")), Letra::Corpo, Cor::Aviso));

    // As citações: sem elas não há conclusão à vista. Se não se encontrou nenhuma linha, diz-se.
    v->addSpacing(10);
    auto *rodape = new QHBoxLayout;
    rodape->setSpacing(6);
    if (citadas.isEmpty()) {
        rodape->addWidget(rotuloMeta(evidencia_->total() ? QStringLiteral("Sem linha encontrada na evidência") : QStringLiteral("Evidência indisponível"),
                                     Tema::cor(Cor::TextoSecundario)));
    } else {
        const auto grupos = intervalos(citadas);
        const int mostrar = std::min<int>(int(grupos.size()), 6);
        for (int i = 0; i < mostrar; ++i) {
            auto *chip = new Chip(nomeDoIntervalo(grupos[i], evidencia_->total()));
            const auto [de, ate] = grupos[i];
            connect(chip, &QPushButton::clicked, this, [this, de, ate] { evidencia_->acender(de, ate); });
            rodape->addWidget(chip);
        }
        if (grupos.size() > mostrar)
            rodape->addWidget(rotuloMeta(QStringLiteral("+%1").arg(grupos.size() - mostrar), Tema::cor(Cor::Claude)));
    }
    rodape->addStretch(1);
    rodape->addWidget(rotuloMeta(QStringLiteral("Confiança ") + a.value(QStringLiteral("confianca")).toString(), Tema::cor(Cor::TextoSecundario)));
    v->addLayout(rodape);
    return c;
}

QWidget *JanelaPrincipal::cartaoDaPergunta(const QJsonObject &p)
{
    auto *c = new QWidget;
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    v->addWidget(etiqueta(p.value(QStringLiteral("pergunta")).toString(), Letra::CorpoForte));

    if (p.value(QStringLiteral("respondida")).toBool()) {
        // Mostra o que o dono já disse, para se perceber porque não se volta a perguntar.
        const QString dito = p.value(QStringLiteral("resposta")).toObject().value(QStringLiteral("texto")).toString();
        v->addWidget(etiqueta(dito.isEmpty() ? QStringLiteral("✓ Já respondeste.") : QStringLiteral("✓ Já respondeste: «%1»").arg(dito), Letra::Corpo, Cor::TextoSecundario));
        return c;
    }
    const int numero = p.value(QStringLiteral("n")).toInt();
    auto *linha = new QHBoxLayout;
    linha->setSpacing(8);
    auto *caixa = new QLineEdit;
    caixa->setPlaceholderText(QStringLiteral("A tua resposta"));
    caixa->setMaxLength(500);
    auto *guardar = new Botao(QStringLiteral("Guardar"));
    linha->addWidget(caixa, 1);
    linha->addWidget(guardar);
    v->addLayout(linha);

    const auto enviar = [this, numero, caixa, guardar] {
        const QString texto = caixa->text().trimmed();
        if (texto.isEmpty())
            return;
        caixa->setEnabled(false);
        guardar->setEnabled(false);
        responder(numero, texto);
    };
    connect(guardar, &QPushButton::clicked, this, enviar);
    connect(caixa, &QLineEdit::returnPressed, this, enviar);
    return c;
}
