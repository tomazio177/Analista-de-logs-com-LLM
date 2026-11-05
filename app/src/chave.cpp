#include "chave.h"

#include "componentes.h"
#include "motor.h"
#include "tema.h"
#include "texto.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

using Tema::Cor;

namespace {

void pintar(QLabel *l, Cor cor)
{
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, Tema::cor(cor));
    l->setPalette(p);
}

} // namespace

DialogoChave::DialogoChave(Motor *motor, bool primeiraVez, QWidget *pai) : QDialog(pai), motor_(motor)
{
    setWindowTitle(QStringLiteral("Chave da API"));
    setMinimumWidth(480);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 22, 24, 20);
    v->setSpacing(12);
    v->addWidget(rotuloMeta(primeiraVez ? QStringLiteral("Bem-vindo") : QStringLiteral("Chave da API"), Tema::cor(Cor::TextoSecundario)));
    auto *explicacao = new QLabel(QStringLiteral("Para explicar o que encontra, a app usa o Claude, da Anthropic. Para isso precisa de uma chave da API, "
                                                 "que crias em console.anthropic.com, em Settings → API keys.\n\n"
                                                 "A chave fica guardada só neste PC, na tua pasta de dados."));
    explicacao->setTextFormat(Qt::PlainText);
    explicacao->setWordWrap(true);
    v->addWidget(explicacao);

    caixa_ = new CaixaDeTexto;
    caixa_->setEchoMode(QLineEdit::Password);
    caixa_->setPlaceholderText(QStringLiteral("sk-ant-…"));
    caixa_->setFont(Tema::fonte(Tema::Letra::Log));
    v->addWidget(caixa_);
    auto *mostrar = new Interruptor(QStringLiteral("Mostrar a chave"));
    v->addWidget(mostrar, 0, Qt::AlignLeft);
    connect(mostrar, &QAbstractButton::toggled, this, [this](bool sim) { caixa_->setEchoMode(sim ? QLineEdit::Normal : QLineEdit::Password); });

    erro_ = new QLabel;
    erro_->setTextFormat(Qt::PlainText);
    erro_->setWordWrap(true);
    erro_->hide();
    v->addWidget(erro_);

    v->addSpacing(6);
    auto *botoes = new QHBoxLayout;
    botoes->addStretch(1);
    auto *cancelar = new Botao(primeiraVez ? QStringLiteral("Agora não") : QStringLiteral("Cancelar"));
    guardar_ = new Botao(QStringLiteral("Guardar"), Botao::Tipo::Primario);
    guardar_->setDefault(true);
    botoes->addWidget(cancelar);
    botoes->addWidget(guardar_);
    v->addLayout(botoes);

    // O "Guardar" não fecha o diálogo: só fecha quando o motor confirmar que guardou.
    connect(guardar_, &QPushButton::clicked, this, &DialogoChave::guardar);
    connect(cancelar, &QPushButton::clicked, this, &QDialog::reject);
    connect(motor_, &Motor::terminou, this, &DialogoChave::aoTerminar);
    connect(motor_, &Motor::falhou, this, &DialogoChave::aoFalhar);
}

void DialogoChave::guardar()
{
    const QString chave = caixa_->text().trimmed();
    if (chave.isEmpty()) {
        mostrarErro(QStringLiteral("Cola aqui a chave."));
        return;
    }
    aEspera_ = true;
    guardar_->setEnabled(false);
    caixa_->setEnabled(false);
    pintar(erro_, Cor::TextoSecundario);
    erro_->setText(QStringLiteral("A confirmar a chave com a Anthropic…"));
    erro_->show();
    motor_->executar(QStringLiteral("config.js"), {QStringLiteral("--guardar-chave")}, chave.toUtf8());
}

void DialogoChave::aoTerminar(const QString &script, const QJsonObject &r)
{
    if (script != QLatin1String("config.js") || !aEspera_)
        return;
    aEspera_ = false;
    if (r.value(QStringLiteral("ok")).toBool()) {
        aviso_ = limparTexto(r.value(QStringLiteral("aviso")).toString());
        accept();
        return;
    }
    mostrarErro(r.value(QStringLiteral("erro")).toObject().value(QStringLiteral("mensagem")).toString());
}

void DialogoChave::aoFalhar(const QString &script, const QString &mensagem)
{
    if (script != QLatin1String("config.js") || !aEspera_)
        return;
    aEspera_ = false;
    mostrarErro(mensagem);
}

void DialogoChave::mostrarErro(const QString &texto)
{
    // Âmbar: algo precisa de ti. O vermelho fica só para a gravidade dos achados.
    pintar(erro_, Cor::Aviso);
    erro_->setText(limparTexto(texto));
    erro_->show();
    guardar_->setEnabled(true);
    caixa_->setEnabled(true);
    caixa_->setFocus();
}
