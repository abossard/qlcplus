/*
  Q Light Controller Plus
  mcp_authoring_probe_plugin.h
*/

#ifndef MCP_AUTHORING_PROBE_PLUGIN_H
#define MCP_AUTHORING_PROBE_PLUGIN_H

#include "qlcioplugin.h"

class McpAuthoringProbePlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

    Q_PROPERTY(int initCount READ initCount)
    Q_PROPERTY(int openCount READ openCount)
    Q_PROPERTY(int closeCount READ closeCount)
    Q_PROPERTY(int writeCount READ writeCount)
    Q_PROPERTY(int feedbackCount READ feedbackCount)

public:
    ~McpAuthoringProbePlugin() override = default;

    void init() override;
    QString name() const override;
    int capabilities() const override;
    QString pluginInfo() const override;

    bool openOutput(quint32 output, quint32 universe) override;
    void closeOutput(quint32 output, quint32 universe) override;
    QStringList outputs() override;
    QString outputInfo(quint32 output) override;
    void writeUniverse(quint32 universe, quint32 output,
                       const QByteArray &data, bool dataChanged) override;

    bool openInput(quint32 input, quint32 universe) override;
    void closeInput(quint32 input, quint32 universe) override;
    QStringList inputs() override;
    QString inputInfo(quint32 input) override;
    void sendFeedBack(quint32 universe, quint32 inputLine,
                      quint32 channel, uchar value, const QVariant &params) override;

    int initCount() const { return m_initCount; }
    int openCount() const { return m_openCount; }
    int closeCount() const { return m_closeCount; }
    int writeCount() const { return m_writeCount; }
    int feedbackCount() const { return m_feedbackCount; }

    Q_INVOKABLE void resetCounters();

private:
    int m_initCount = 0;
    int m_openCount = 0;
    int m_closeCount = 0;
    int m_writeCount = 0;
    int m_feedbackCount = 0;
};

#endif // MCP_AUTHORING_PROBE_PLUGIN_H
