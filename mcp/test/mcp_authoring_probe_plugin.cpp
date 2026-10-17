/*
  Q Light Controller Plus
  mcp_authoring_probe_plugin.cpp
*/

#include "mcp_authoring_probe_plugin.h"

void McpAuthoringProbePlugin::init()
{
    ++m_initCount;
}

QString McpAuthoringProbePlugin::name() const
{
    return QStringLiteral("MCP Authoring Probe Plugin");
}

int McpAuthoringProbePlugin::capabilities() const
{
    return QLCIOPlugin::Input | QLCIOPlugin::Output | QLCIOPlugin::Feedback;
}

QString McpAuthoringProbePlugin::pluginInfo() const
{
    return QStringLiteral("Probe plugin used by MCP authoring owner test.");
}

bool McpAuthoringProbePlugin::openOutput(quint32 output, quint32 universe)
{
    ++m_openCount;
    addToMap(universe, output, Output);
    return true;
}

void McpAuthoringProbePlugin::closeOutput(quint32 output, quint32 universe)
{
    Q_UNUSED(output)
    ++m_closeCount;
    removeFromMap(universe, output, Output);
}

QStringList McpAuthoringProbePlugin::outputs()
{
    return QStringList{QStringLiteral("Probe Output 1")};
}

QString McpAuthoringProbePlugin::outputInfo(quint32 output)
{
    Q_UNUSED(output)
    return QStringLiteral("Probe output line");
}

void McpAuthoringProbePlugin::writeUniverse(quint32 universe, quint32 output,
                                            const QByteArray &data, bool dataChanged)
{
    Q_UNUSED(universe)
    Q_UNUSED(output)
    Q_UNUSED(data)
    Q_UNUSED(dataChanged)
    ++m_writeCount;
}

bool McpAuthoringProbePlugin::openInput(quint32 input, quint32 universe)
{
    ++m_openCount;
    addToMap(universe, input, Input);
    return true;
}

void McpAuthoringProbePlugin::closeInput(quint32 input, quint32 universe)
{
    Q_UNUSED(input)
    ++m_closeCount;
    removeFromMap(universe, input, Input);
}

QStringList McpAuthoringProbePlugin::inputs()
{
    return QStringList{QStringLiteral("Probe Input 1")};
}

QString McpAuthoringProbePlugin::inputInfo(quint32 input)
{
    Q_UNUSED(input)
    return QStringLiteral("Probe input line");
}

void McpAuthoringProbePlugin::sendFeedBack(quint32 universe, quint32 inputLine,
                                           quint32 channel, uchar value, const QVariant &params)
{
    Q_UNUSED(universe)
    Q_UNUSED(inputLine)
    Q_UNUSED(channel)
    Q_UNUSED(value)
    Q_UNUSED(params)
    ++m_feedbackCount;
}

void McpAuthoringProbePlugin::resetCounters()
{
    m_openCount = 0;
    m_closeCount = 0;
    m_writeCount = 0;
    m_feedbackCount = 0;
}
