#include "chave.h"

#include "motor.h"
#include "texto.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

DialogoChave::DialogoChave(Motor *motor, bool primeiraVez, QWidget *pai) : QDialog(pai), motor_(motor)
{
    setWindowTitle(QStringLiteral("Chave da API"));
    setMinimumWidth(460);

    auto *v = new QVBoxLayout(this);
    v->setSpacing(10);
    auto *explicacao = new QLabel((primeiraVez ? QStringLiteral("Bem-vindo. ") : QString())
                                  + QStringLiteral("Para explicar o que encontra, a app usa o Claude, da Anthropic. Para isso precisa de uma chave da API, "
                                                   "que crias em console.anthropic.com, em Settings → API keys.\n\n"
                                                   "A chave fica guardada só neste PC, na tua pasta de dados."));
    explicacao->setTextFormat(Qt::PlainText);
    explicacao->setWordWrap(true);
    v->addWidget(explicacao);

    caixa_ = new QLineEdit;
    caixa_->setEchoMode(QLineEdit::Password);
    caixa_->setPlaceholderText(QStringLiteral("sk-ant-…"));
    v->addWidget(caixa_);
    auto *mostrar = new QCheckBox(QStringLiteral("Mostrar a chave"));
    v->addWidget(mostrar);
    connect(mostrar, &QCheckBox::toggled, this, [this](bool sim) { caixa_->setEchoMode(sim ? QLineEdit::Normal : QLineEdit::Password); });

    erro_ = new QLabel;
    erro_->setTextFormat(Qt::PlainText);
    erro_->setWordWrap(true);
    erro_->setStyleSheet(QStringLiteral("color: #b3261e;"));
    erro_->hide();
    v->addWidget(erro_);

    auto *botoes = new QDialogButtonBox;
    guardar_ = botoes->addButton(QStringLiteral("Guardar"), QDialogButtonBox::AcceptRole);
    botoes->addButton(primeiraVez ? QStringLiteral("Agora não") : QStringLiteral("Cancelar"), QDialogButtonBox::RejectRole);
    guardar_->setDefault(true);
    v->addWidget(botoes);

    // O "Guardar" não fecha o diálogo: só fecha quando o motor confirmar que guardou.
    connect(botoes, &QDialogButtonBox::accepted, this, &DialogoChave::guardar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);
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
    erro_->setStyleSheet(QString());
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
    erro_->setStyleSheet(QStringLiteral("color: #b3261e;"));
    erro_->setText(limparTexto(texto));
    erro_->show();
    guardar_->setEnabled(true);
    caixa_->setEnabled(true);
    caixa_->setFocus();
}
