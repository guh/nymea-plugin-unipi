/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2019 Bernhard Trinnes <bernhard.trinnes@nymea.io>        *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation; either           *
 *  version 2.1 of the License, or (at your option) any later version.     *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef NEURON_H
#define NEURON_H

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QtSerialBus>
#include <QUuid>

class Neuron : public QObject
{
    Q_OBJECT
public:

    enum NeuronTypes {
        S103,
        M103,
        M203,
        M303,
        M403,
        M503,
        L203,
        L303,
        L403,
        L503,
        L513
    };

    explicit Neuron(QModbusTcpClient *modbusInterface, QObject *parent = nullptr);
    ~Neuron();

    bool init();
    QString type();
    QString firmwareVersion();
    bool firmwareUpdateAvailable();
    void startFirmwareUpdate();

    QList<QString> digitalInputs();
    QList<QString> digitalOutputs();
    QList<QString> analogInputs();
    QList<QString> analogOutputs();
    QList<QString> userLEDs();

    QUuid setDigitalOutput(const QString &circuit, bool value);
    QUuid setAnalogOutput(const QString &circuit, double value);
    QUuid setUserLED(const QString &circuit, bool value);

    bool getDigitalOutput(const QString &circuit);
    bool getDigitalInput(const QString &circuit);

    bool getAnalogOutput(const QString &circuit);
    bool getAnalogInput(const QString &circuit);

    bool getAllDigitalOutputs();
    bool getAllDigitalInputs();
    bool getAllAnalogInputs();
    bool getAllAnalogOutputs();

    bool getUserLED(const QString &circuit);
private:
    int m_slaveAddress = 0;
    uint m_responseTimeoutTime = 2000;

    QTimer *m_inputPollingTimer = nullptr;
    QTimer *m_outputPollingTimer = nullptr;

    QModbusTcpClient *m_modbusInterface = nullptr;

    QHash<QString, int> m_modbusDigitalOutputRegisters;
    QHash<QString, int> m_modbusDigitalInputRegisters;
    QHash<QString, int> m_modbusAnalogInputRegisters;
    QHash<QString, int> m_modbusAnalogOutputRegisters;
    QHash<QString, int> m_modbusUserLEDRegisters;
    QList<QPair<QUuid, QModbusDataUnit>> m_writeRequestQueue;
    QList<QModbusDataUnit> m_readRequestQueue;

    NeuronTypes m_neuronType = NeuronTypes::S103;

    QHash<int, uint16_t> m_previousModbusRegisterValue;

    bool loadModbusMap();
    bool modbusReadRequest(QModbusDataUnit request);
    bool modbusWriteRequest(QUuid requestId, QModbusDataUnit request);

    bool getInputRegisters(QList<int> registers);
    bool getHoldingRegisters(QList<int> registers);
    bool getCoils(QList<int> registers);

signals:
    void requestExecuted(QUuid requestId, bool success);
    void requestError(QUuid requestId, const QString &error);
    void digitalInputStatusChanged(QString &circuit, bool value);
    void digitalOutputStatusChanged(QString &circuit, bool value);
    void analogInputStatusChanged(QString &circuit, double value);
    void analogOutputStatusChanged(QString &circuit, double value);
    void userLEDStatusChanged(QString &circuit, bool value);
    void connectionStateChanged(bool state);

public slots:
    void onOutputPollingTimer();
    void onInputPollingTimer();
};

#endif // NEURON_H
