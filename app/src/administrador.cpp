#include "administrador.h"

#include <QDir>

QString linhaDeComandos(const QStringList &args)
{
    QStringList partes;
    for (const QString &a : args) {
        if (!a.isEmpty() && !a.contains(QLatin1Char(' ')) && !a.contains(QLatin1Char('\t')) && !a.contains(QLatin1Char('"'))) {
            partes << a;
            continue;
        }
        // Regras do Windows: as barras antes de uma aspa (ou do fim) duplicam-se, e a aspa leva uma barra à frente.
        QString r = QStringLiteral("\"");
        int barras = 0;
        for (const QChar c : a) {
            if (c == QLatin1Char('\\')) {
                ++barras;
                continue;
            }
            if (c == QLatin1Char('"')) {
                r += QString(barras * 2 + 1, QLatin1Char('\\'));
                r += c;
            } else {
                r += QString(barras, QLatin1Char('\\'));
                r += c;
            }
            barras = 0;
        }
        r += QString(barras * 2, QLatin1Char('\\'));
        r += QLatin1Char('"');
        partes << r;
    }
    return partes.join(QLatin1Char(' '));
}

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

bool processoElevado()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevacao{};
    DWORD tamanho = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevacao, sizeof(elevacao), &tamanho);
    CloseHandle(token);
    return ok && elevacao.TokenIsElevated;
}

int correrComoAdministrador(const QString &programa, const QStringList &args, const QString &pasta)
{
    const std::wstring ficheiro = QDir::toNativeSeparators(programa).toStdWString();
    const std::wstring parametros = linhaDeComandos(args).toStdWString();
    const std::wstring diretorio = QDir::toNativeSeparators(pasta).toStdWString();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas"; // é isto que faz o Windows pedir autorização
    info.lpFile = ficheiro.c_str();
    info.lpParameters = parametros.c_str();
    info.lpDirectory = diretorio.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess)
        return -1; // inclui a pessoa ter carregado em "Não"

    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD codigo = 1;
    GetExitCodeProcess(info.hProcess, &codigo);
    CloseHandle(info.hProcess);
    return static_cast<int>(codigo);
}

#else

bool processoElevado()
{
    return true;
}

int correrComoAdministrador(const QString &, const QStringList &, const QString &)
{
    return -1;
}

#endif
