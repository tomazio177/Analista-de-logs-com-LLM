#include "janela.h"

#include "motor.h"
#include "texto.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTime>
#include <QVBoxLayout>
#include <algorithm>

namespace {

// Tudo o que vem do motor chega como texto simples: um QLabel interpreta HTML por defeito, e um aparelho chamado
// "<img src=...>" seria desenhado em vez de lido. Por isso o formato é sempre PlainText, e o texto passa por limparTexto.
QLabel *etiqueta(const QString &texto, bool negrito = false, bool suave = false)
{
    auto *l = new QLabel(limparTexto(texto));
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (negrito || suave) {
        QFont f = l->font();
        f.setBold(negrito);
        l->setFont(f);
    }
    if (suave)
        l->setStyleSheet(QStringLiteral("color: palette(placeholder-text);"));
    return l;
}

QLabel *pilula(const QString &texto, const QString &cor)
{
    auto *l = new QLabel(texto);
    l->setTextFormat(Qt::PlainText);
    l->setStyleSheet(QStringLiteral("background: %1; color: white; border-radius: 9px; padding: 2px 10px; font-weight: bold;").arg(cor));
    l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return l;
}

void esvaziar(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }
}

QString corDaGravidade(const QString &g)
{
    if (g == QLatin1String("alta")) return QStringLiteral("#b3261e");
    if (g == QLatin1String("media")) return QStringLiteral("#b26a00");
    if (g == QLatin1String("baixa")) return QStringLiteral("#3b6ea5");
    return QStringLiteral("#6b7785");
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
    return t.isValid() ? QLocale().toString(t.toLocalTime(), QLocale::ShortFormat) : QString();
}

QFrame *cartao()
{
    auto *c = new QFrame;
    c->setObjectName(QStringLiteral("cartao"));
    c->setStyleSheet(QStringLiteral("QFrame#cartao { border: 1px solid palette(mid); border-radius: 8px; }"));
    return c;
}

QStringList lista(const QJsonValue &v)
{
    QStringList r;
    for (const QJsonValue &x : v.toArray())
        r << x.toString();
    return r;
}

} // namespace

JanelaPrincipal::JanelaPrincipal(Motor *motor, QWidget *pai) : QMainWindow(pai), motor_(motor)
{
    setWindowTitle(QStringLiteral("Analista de logs"));
    resize(780, 720);
    setMinimumSize(560, 480);

    auto *centro = new QWidget;
    auto *raiz = new QVBoxLayout(centro);
    raiz->setContentsMargins(16, 14, 16, 12);
    raiz->setSpacing(10);

    // Topo: o nível geral, a data da triagem e o botão.
    auto *topo = new QHBoxLayout;
    nivel_ = pilula(QStringLiteral("—"), QStringLiteral("#6b7785"));
    auto *colunaTitulo = new QVBoxLayout;
    colunaTitulo->setSpacing(0);
    auto *titulo = new QLabel(QStringLiteral("Analista de logs"));
    QFont f = titulo->font();
    f.setPointSizeF(f.pointSizeF() * 1.5);
    f.setBold(true);
    titulo->setFont(f);
    subtitulo_ = etiqueta(QString(), false, true);
    colunaTitulo->addWidget(titulo);
    colunaTitulo->addWidget(subtitulo_);
    botao_ = new QPushButton(QStringLiteral("Verificar agora"));
    botao_->setDefault(true);
    botao_->setMinimumHeight(34);
    topo->addWidget(nivel_, 0, Qt::AlignVCenter);
    topo->addLayout(colunaTitulo, 1);
    topo->addWidget(botao_, 0, Qt::AlignVCenter);
    raiz->addLayout(topo);

    estado_ = etiqueta(QString(), false, true);
    estado_->hide();
    progresso_ = new QProgressBar;
    progresso_->setTextVisible(false);
    progresso_->setMaximumHeight(8);
    progresso_->hide();
    raiz->addWidget(estado_);
    raiz->addWidget(progresso_);

    auto *containerFaixas = new QWidget;
    faixas_ = new QVBoxLayout(containerFaixas);
    faixas_->setContentsMargins(0, 0, 0, 0);
    faixas_->setSpacing(6);
    raiz->addWidget(containerFaixas);

    // O conteúdo desliza; o topo fica sempre à vista.
    auto *conteudo = new QWidget;
    auto *c = new QVBoxLayout(conteudo);
    c->setContentsMargins(0, 4, 8, 4);
    c->setSpacing(12);
    resumo_ = etiqueta(QString());
    c->addWidget(resumo_);
    avisos_ = new QVBoxLayout;
    avisos_->setSpacing(6);
    c->addLayout(avisos_);
    achados_ = new QVBoxLayout;
    achados_->setSpacing(10);
    c->addLayout(achados_);
    tituloPerguntas_ = etiqueta(QStringLiteral("Perguntas que só tu sabes responder"), true);
    tituloPerguntas_->hide();
    c->addWidget(tituloPerguntas_);
    perguntas_ = new QVBoxLayout;
    perguntas_->setSpacing(10);
    c->addLayout(perguntas_);
    c->addStretch(1);

    auto *rolo = new QScrollArea;
    rolo->setWidgetResizable(true);
    rolo->setFrameShape(QFrame::NoFrame);
    rolo->setWidget(conteudo);
    raiz->addWidget(rolo, 1);
    setCentralWidget(centro);

    connect(botao_, &QPushButton::clicked, this, [this] { verificarAgora(); });
    connect(motor_, &Motor::terminou, this, &JanelaPrincipal::aoTerminar);
    connect(motor_, &Motor::falhou, this, &JanelaPrincipal::aoFalhar);
    connect(motor_, &Motor::progresso, this, &JanelaPrincipal::aoProgredir);

    if (!motor_->localizacao().ok()) {
        botao_->setEnabled(false);
        faixa(motor_->localizacao().erro, Faixa::Erro);
    }
    estadoVazio(QStringLiteral("A abrir…"));
}

// --- Pedidos ao motor -------------------------------------------------------------------------------------

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
    definirEstado(QStringLiteral("A verificar…"));
    motor_->executar(QStringLiteral("vigiar.js"), aprovar ? QStringList{QStringLiteral("--aprovar")} : QStringList{});
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
            estadoVazio(QStringLiteral("Ainda não há nenhuma triagem. Carrega em «Verificar agora»."));
        else
            faixa(r.value(QStringLiteral("erro")).toObject().value(QStringLiteral("mensagem")).toString(), Faixa::Erro);
        emit carregou();
    } else if (script == QLatin1String("notas.js")) {
        if (r.value(QStringLiteral("ok")).toBool())
            carregarUltima();
        else
            faixa(r.value(QStringLiteral("erro")).toObject().value(QStringLiteral("mensagem")).toString(), Faixa::Erro);
    }
}

void JanelaPrincipal::aoFalhar(const QString &script, const QString &mensagem)
{
    if (script == QLatin1String("vigiar.js"))
        definirOcupado(false);
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
    } else if (fase == QLatin1String("descobrir")) {
        progresso_->setRange(0, e.value(QStringLiteral("total")).toInt());
        progresso_->setValue(e.value(QStringLiteral("feitos")).toInt());
        definirEstado(QStringLiteral("A procurar aparelhos… %1/%2").arg(e.value(QStringLiteral("feitos")).toInt()).arg(e.value(QStringLiteral("total")).toInt()));
    } else if (fase == QLatin1String("portas")) {
        progresso_->setRange(0, 0); // daqui em diante o motor não diz quanto falta
        definirEstado(QStringLiteral("A testar as portas de %1 aparelhos. Depois lê os logins e pede a triagem, o que pode demorar um minuto.")
                             .arg(e.value(QStringLiteral("aparelhos")).toInt()));
    }
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
    if (ok(logins)) {
        resumo << QStringLiteral("%1 logins novos").arg(logins.value(QStringLiteral("novos")).toInt());
        if (!logins.value(QStringLiteral("periodos_possivelmente_incompletos")).toArray().isEmpty())
            faixa(QStringLiteral("O Windows já apagou alguns logins falhados antigos antes de serem lidos. Os que aparecem são reais, mas pode faltar algum."), Faixa::Info);
    } else if (codigo(logins) == QLatin1String("sem-permissao")) {
        faixa(QStringLiteral("Os logins falhados do Windows precisam de administrador. Fecha a app e abre-a com botão direito → «Executar como administrador»."), Faixa::Aviso);
    } else if (codigo(logins) != QLatin1String("nao-windows")) {
        faixa(mensagem(logins), Faixa::Aviso);
    }

    const QJsonObject triagem = parte("triagem");
    if (ok(triagem)) {
        mostrarTriagem(triagem);
    } else {
        QString texto = mensagem(triagem);
        const QString detalhe = triagem.value(QStringLiteral("detalhe")).toString();
        if (!detalhe.isEmpty())
            texto += QStringLiteral("\n") + detalhe;
        faixa(texto, Faixa::Erro);
    }
    definirEstado(resumo.isEmpty() ? QString()
                                      : QStringLiteral("Verificado às %1: %2.").arg(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat), resumo.join(QStringLiteral(", "))));
}

// --- O que se vê ------------------------------------------------------------------------------------------

void JanelaPrincipal::definirOcupado(bool ocupado)
{
    ocupado_ = ocupado;
    botao_->setEnabled(!ocupado && motor_->localizacao().ok());
    botao_->setText(ocupado ? QStringLiteral("A verificar…") : QStringLiteral("Verificar agora"));
    progresso_->setVisible(ocupado);
    if (ocupado)
        progresso_->setRange(0, 0);
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
    static const char *const cores[] = {"background: #e3eefb; color: #12304f;", "background: #fff3cd; color: #4d3b00;", "background: #fde7e9; color: #5c0b12;"};
    auto *f = new QFrame;
    f->setObjectName(QStringLiteral("faixa"));
    f->setStyleSheet(QStringLiteral("QFrame#faixa { %1 border-radius: 6px; } QFrame#faixa QLabel { %1 }").arg(QLatin1String(cores[static_cast<int>(tipo)])));
    auto *linha = new QHBoxLayout(f);
    linha->setContentsMargins(10, 6, 10, 6);
    linha->addWidget(etiqueta(texto), 1);
    if (!botao.isEmpty()) {
        auto *b = new QPushButton(botao);
        connect(b, &QPushButton::clicked, this, [acao] { acao(); });
        linha->addWidget(b);
    }
    faixas_->addWidget(f);
}

void JanelaPrincipal::estadoVazio(const QString &texto)
{
    nivel_->setText(QStringLiteral("—"));
    nivel_->setStyleSheet(QStringLiteral("background: #6b7785; color: white; border-radius: 9px; padding: 2px 10px; font-weight: bold;"));
    subtitulo_->setText(QString());
    resumo_->setText(texto);
    esvaziar(avisos_);
    esvaziar(achados_);
    esvaziar(perguntas_);
    tituloPerguntas_->hide();
}

void JanelaPrincipal::mostrarTriagem(const QJsonObject &t)
{
    estadoVazio(QString());

    const QString nivel = t.value(QStringLiteral("nivel_geral")).toString();
    const bool tranquilo = nivel == QLatin1String("tranquilo");
    const QString texto = tranquilo ? QStringLiteral("TRANQUILO") : nivel == QLatin1String("urgente") ? QStringLiteral("URGENTE") : QStringLiteral("ATENÇÃO");
    const QString cor = tranquilo ? QStringLiteral("#1d7448") : nivel == QLatin1String("urgente") ? QStringLiteral("#b3261e") : QStringLiteral("#b26a00");
    nivel_->setText(texto);
    nivel_->setStyleSheet(QStringLiteral("background: %1; color: white; border-radius: 9px; padding: 2px 10px; font-weight: bold;").arg(cor));

    const QString quando = horaLocal(t.value(QStringLiteral("data")).toString());
    subtitulo_->setText(quando.isEmpty() ? QString() : QStringLiteral("Triagem de %1").arg(quando));
    resumo_->setText(limparTexto(t.value(QStringLiteral("resumo")).toString()));

    for (const QString &aviso : lista(t.value(QStringLiteral("avisos"))))
        avisos_->addWidget(etiqueta(QStringLiteral("⚠ ") + aviso, true));

    QList<QJsonObject> achados;
    for (const QJsonValue &a : t.value(QStringLiteral("achados")).toArray())
        achados << a.toObject();
    std::stable_sort(achados.begin(), achados.end(), [](const QJsonObject &x, const QJsonObject &y) {
        return ordemDaGravidade(x.value(QStringLiteral("gravidade")).toString()) < ordemDaGravidade(y.value(QStringLiteral("gravidade")).toString());
    });
    for (const QJsonObject &a : achados)
        achados_->addWidget(cartaoDoAchado(a));

    const QJsonArray perguntas = t.value(QStringLiteral("perguntas")).toArray();
    tituloPerguntas_->setVisible(!perguntas.isEmpty());
    for (const QJsonValue &p : perguntas)
        perguntas_->addWidget(cartaoDaPergunta(p.toObject()));
}

QWidget *JanelaPrincipal::cartaoDoAchado(const QJsonObject &a)
{
    auto *c = cartao();
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(12, 10, 12, 12);
    v->setSpacing(4);

    const QString gravidade = a.value(QStringLiteral("gravidade")).toString();
    auto *cabeca = new QHBoxLayout;
    cabeca->addWidget(pilula(nomeDaGravidade(gravidade), corDaGravidade(gravidade)), 0, Qt::AlignTop);
    cabeca->addWidget(etiqueta(a.value(QStringLiteral("titulo")).toString(), true), 1);
    cabeca->addWidget(etiqueta(QStringLiteral("confiança %1").arg(a.value(QStringLiteral("confianca")).toString()), false, true), 0, Qt::AlignTop);
    v->addLayout(cabeca);

    if (a.contains(QStringLiteral("gravidade_do_modelo")))
        v->addWidget(etiqueta(QStringLiteral("O modelo deu %1; num alerta vindo da rede, as regras não deixam baixar de %2.")
                                  .arg(nomeDaGravidade(a.value(QStringLiteral("gravidade_do_modelo")).toString()).toLower(), nomeDaGravidade(gravidade).toLower()),
                              false, true));

    const auto seccao = [v](const QString &titulo, const QStringList &linhas) {
        if (linhas.isEmpty())
            return;
        v->addSpacing(4);
        v->addWidget(etiqueta(titulo, true));
        for (const QString &l : linhas)
            v->addWidget(etiqueta(QStringLiteral("·  ") + l));
    };
    seccao(QStringLiteral("Nos dados"), lista(a.value(QStringLiteral("nos_dados"))));
    seccao(QStringLiteral("Conhecimento geral (não vem dos teus dados)"), lista(a.value(QStringLiteral("conhecimento_geral"))));
    seccao(QStringLiteral("Conclusão"), {a.value(QStringLiteral("conclusao")).toString()});
    seccao(QStringLiteral("O que fazer"), {a.value(QStringLiteral("acao_sugerida")).toString()});

    const QStringList semApoio = lista(a.value(QStringLiteral("factos_sem_apoio")));
    if (!semApoio.isEmpty()) {
        v->addSpacing(4);
        v->addWidget(etiqueta(QStringLiteral("⚠ Citado como facto, mas não está nos dados enviados: ") + semApoio.join(QStringLiteral(", ")), true));
    }
    return c;
}

QWidget *JanelaPrincipal::cartaoDaPergunta(const QJsonObject &p)
{
    auto *c = cartao();
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(6);
    v->addWidget(etiqueta(p.value(QStringLiteral("pergunta")).toString(), true));

    if (p.value(QStringLiteral("respondida")).toBool()) {
        v->addWidget(etiqueta(QStringLiteral("✓ Respondida. A próxima triagem já sabe."), false, true));
        return c;
    }
    const int numero = p.value(QStringLiteral("n")).toInt();
    auto *linha = new QHBoxLayout;
    auto *caixa = new QLineEdit;
    caixa->setPlaceholderText(QStringLiteral("A tua resposta"));
    caixa->setMaxLength(500);
    auto *guardar = new QPushButton(QStringLiteral("Guardar"));
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
