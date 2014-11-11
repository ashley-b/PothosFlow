// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EnvironmentEval.hpp"
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <Pothos/System/Logger.hpp>
#include <Poco/URI.h>
#include <Poco/Logger.h>
#include <sstream>

EnvironmentEval::EnvironmentEval(void):
    _failureState(false)
{
    return;
}

EnvironmentEval::~EnvironmentEval(void)
{
    return;
}

void EnvironmentEval::acceptConfig(const QString &zoneName, const Poco::JSON::Object::Ptr &config)
{
    _zoneName = zoneName;
    _config = config;
}

void EnvironmentEval::update(void)
{
    if (this->isFailureState()) return;

    try
    {
        //env already exists, try test communication
        if (_env)
        {
            _env->findProxy("Pothos/Util/EvalEnvironment");
        }
        //otherwise, make a new env
        else
        {
            _env = this->makeEnvironment();
            auto EvalEnvironment = _env->findProxy("Pothos/Util/EvalEnvironment");
            _eval = EvalEnvironment.callProxy("make");
        }
    }
    catch (const Pothos::Exception &ex)
    {
        poco_error(Poco::Logger::get("PothosGui.EnvironmentEval.update"), ex.displayText());
        _failureState = true;

        _errorMsg = tr("Remote environment crashed"); //default error message

        //determine if the remote host is offline for a better error message
        const auto hostUri = getHostProcFromConfig(_zoneName, _config).first;
        try
        {
            Pothos::RemoteClient client(hostUri);
        }
        catch(const Pothos::RemoteClientError &)
        {
            _errorMsg = tr("Remote host %1 is offline").arg(QString::fromStdString(hostUri));
        }
    }
}

HostProcPair EnvironmentEval::getHostProcFromConfig(const QString &zoneName, const Poco::JSON::Object::Ptr &config)
{
    if (zoneName == "gui") return HostProcPair("gui://localhost", "gui");

    auto hostUri = config?config->getValue<std::string>("hostUri"):"tcp://localhost";
    auto processName = config?config->getValue<std::string>("processName"):"";
    return HostProcPair(hostUri, processName);
}

Pothos::ProxyEnvironment::Sptr EnvironmentEval::makeEnvironment(void)
{
    if (_zoneName == "gui") return Pothos::ProxyEnvironment::make("managed");

    const auto hostUri = getHostProcFromConfig(_zoneName, _config).first;

    //connect to the remote host and spawn a server
    auto serverEnv = Pothos::RemoteClient(hostUri).makeEnvironment("managed");
    auto serverHandle = serverEnv->findProxy("Pothos/RemoteServer").callProxy("new", "tcp://0.0.0.0");

    //construct the uri for the new server
    auto actualPort = serverHandle.call<std::string>("getActualPort");
    Poco::URI newHostUri(hostUri);
    newHostUri.setPort(std::stoul(actualPort));

    //connect the client environment
    auto client = Pothos::RemoteClient(newHostUri.toString());
    client.holdRef(Pothos::Object(serverHandle));
    auto env = client.makeEnvironment("managed");

    //setup log delivery from the server process
    const auto syslogListenPort = Pothos::System::Logger::startSyslogListener();
    const auto serverAddr = env->getPeeringAddress() + ":" + syslogListenPort;
    env->findProxy("Pothos/System/Logger").callVoid("startSyslogForwarding", serverAddr);
    const auto logSource = (not _zoneName.isEmpty())? _zoneName.toStdString() : newHostUri.getHost();
    env->findProxy("Pothos/System/Logger").callVoid("forwardStdIoToLogging", logSource);

    return env;
}