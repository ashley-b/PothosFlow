// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <Pothos/Proxy/Environment.hpp>
#include <QObject>
#include <QString>
#include <memory>
#include <set>

class BlockEngine;

class ZoneEngine : public QObject
{
    Q_OBJECT
public:

    ZoneEngine(QObject *parent);

    ~ZoneEngine(void);

    void evalExpr(const QString &expr);

    bool isEnvironmentAlive(void);

private:
    Pothos::ProxyEnvironment::Sptr _env;
    Pothos::Proxy _eval;
    Pothos::Proxy _threadPool;
    std::set<std::shared_ptr<BlockEngine>> _blockCaches;
};