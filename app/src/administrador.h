#pragma once

#include <QString>
#include <QStringList>

// Ler os logins do Windows precisa de administrador. Em vez de abrir a app inteira com esses poderes, pede-se só
// para o comando que lê os logins: o Windows mostra o aviso "Permitir que esta aplicação faça alterações?".

// Se este processo já tem privilégios de administrador. Fora do Windows devolve sempre true: lá não há este passo.
bool processoElevado();

// Corre `programa args` como administrador, na `pasta`, sem janela, e espera que acabe.
// Devolve o código de saída, ou -1 se a pessoa recusou ou o programa não arrancou. Bloqueia: chamar fora da thread da janela.
int correrComoAdministrador(const QString &programa, const QStringList &args, const QString &pasta);

// Junta os argumentos numa linha de comandos do Windows, com aspas onde for preciso. Fica à vista para os testes.
QString linhaDeComandos(const QStringList &args);
