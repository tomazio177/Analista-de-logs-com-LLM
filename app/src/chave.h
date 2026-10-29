#pragma once

#include <QDialog>
#include <QJsonObject>

class Motor;
class QLabel;
class QLineEdit;
class QPushButton;

// Pede a chave da API e entrega-a ao motor pelo stdin. O motor confirma-a com a Anthropic antes de a guardar,
// e um erro (chave recusada, mal copiada) fica no próprio diálogo para se corrigir sem recomeçar.
class DialogoChave : public QDialog
{
    Q_OBJECT

public:
    DialogoChave(Motor *motor, bool primeiraVez, QWidget *pai = nullptr);

    // Um aviso do motor depois de guardar (por exemplo: sem internet, não deu para confirmar).
    QString aviso() const { return aviso_; }

private:
    void guardar();
    void aoTerminar(const QString &script, const QJsonObject &resultado);
    void aoFalhar(const QString &script, const QString &mensagem);
    void mostrarErro(const QString &texto);

    Motor *motor_;
    QLineEdit *caixa_;
    QLabel *erro_;
    QPushButton *guardar_;
    bool aEspera_ = false;
    QString aviso_;
};
