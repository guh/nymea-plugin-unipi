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

#include "neuronextension.h"
#include "extern-plugininfo.h"

#include <QFile>
#include <QTextStream>
#include <QModbusDataUnit>
#include <QStandardPaths>

NeuronExtension::NeuronExtension(ExtensionTypes extensionType, QModbusRtuSerialMaster *modbusInterface, int slaveAddress, QObject *parent) :
    QObject(parent),
    m_modbusInterface(modbusInterface),
    m_slaveAddress(slaveAddress),
    m_extensionType(extensionType)
{
    m_inputPollingTimer = new QTimer(this);
    connect(m_inputPollingTimer, &QTimer::timeout, this, &NeuronExtension::onInputPollingTimer);
    m_inputPollingTimer->setTimerType(Qt::TimerType::PreciseTimer);
    m_inputPollingTimer->setInterval(200);

    m_outputPollingTimer = new QTimer(this);
    connect(m_outputPollingTimer, &QTimer::timeout, this, &NeuronExtension::onOutputPollingTimer);
    m_outputPollingTimer->setTimerType(Qt::TimerType::PreciseTimer);
    m_outputPollingTimer->setInterval(1000);

    if (m_modbusInterface->state() == QModbusDevice::State::ConnectedState) {
        m_inputPollingTimer->start();
        m_outputPollingTimer->start();
    }

    connect(m_modbusInterface, &QModbusDevice::stateChanged, this, [this] (QModbusDevice::State state) {
        if (state == QModbusDevice::State::ConnectedState) {
            if (m_inputPollingTimer)
                m_inputPollingTimer->start();
            if (m_outputPollingTimer)
                m_outputPollingTimer->start();
            emit connectionStateChanged(true);
        } else {
            if (m_inputPollingTimer)
                m_inputPollingTimer->stop();
            if (m_outputPollingTimer)
                m_outputPollingTimer->stop();
            emit connectionStateChanged(false);
        }
    });
}

NeuronExtension::~NeuronExtension(){
    if (m_inputPollingTimer) {
        m_inputPollingTimer->stop();
        m_inputPollingTimer->deleteLater();
        m_inputPollingTimer = nullptr;
    }
    if (m_outputPollingTimer) {
        m_outputPollingTimer->stop();
        m_outputPollingTimer->deleteLater();
        m_outputPollingTimer = nullptr;
    }
}

bool NeuronExtension::init() {

    if (!loadModbusMap()) {
        return false;
    }

    if (!m_modbusInterface) {
        qWarning(dcUniPi()) << "Modbus RTU interface not available";
        return false;
    }

    if (m_modbusInterface->connectDevice()) {
        qWarning(dcUniPi()) << "Could not connect to RTU device";
        return  false;
    }
    return true;
}

QString NeuronExtension::type()
{
    switch(m_extensionType) {
    case ExtensionTypes::xS10:
        return "xS10";
    case ExtensionTypes::xS20:
        return "xS20";
    case ExtensionTypes::xS30:
        return "xS30";
    case ExtensionTypes::xS40:
        return "xS40";
    case ExtensionTypes::xS50:
        return "xS50";
    }
    return "Unknown";
}

int NeuronExtension::slaveAddress()
{
    return m_slaveAddress;
}

void NeuronExtension::setSlaveAddress(int slaveAddress)
{
    m_slaveAddress = slaveAddress;
}

QList<QString> NeuronExtension::digitalInputs()
{
    return m_modbusDigitalInputRegisters.keys();
}

QList<QString> NeuronExtension::digitalOutputs()
{
    return m_modbusDigitalOutputRegisters.keys();
}

QList<QString> NeuronExtension::analogInputs()
{
    return m_modbusAnalogInputRegisters.keys();
}

QList<QString> NeuronExtension::analogOutputs()
{
    return m_modbusAnalogOutputRegisters.keys();
}

QList<QString> NeuronExtension::userLEDs()
{
    return m_modbusUserLEDRegisters.keys();
}

bool NeuronExtension::loadModbusMap()
{
    QStringList fileCoilList;
    QStringList fileRegisterList;

    switch(m_extensionType) {
    case ExtensionTypes::xS10:
        fileCoilList.append(QString("/Neuron_xS10/Neuron_xS10-Coils-group-1.csv"));
        break;
    case ExtensionTypes::xS20:
        fileCoilList.append(QString("/Neuron_xS20/Neuron_xS20-Coils-group-1.csv"));
        break;
    case ExtensionTypes::xS30:
        fileCoilList.append(QString("/Neuron_xS30/Neuron_xS30-Coils-group-1.csv"));
        break;
    case ExtensionTypes::xS40:
        fileCoilList.append(QString("/Neuron_xS40/Neuron_xS40-Coils-group-1.csv"));
        break;
    case ExtensionTypes::xS50:
        fileCoilList.append(QString("/Neuron_xS50/Neuron_xS50-Coils-group-1.csv"));
        break;
    }

    foreach (QString relativeFilePath, fileCoilList) {
        QString absoluteFilePath = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation).last() + "/nymea/modbus" + relativeFilePath;
        qDebug(dcUniPi()) << "Open CSV File:" << absoluteFilePath;
        QFile *csvFile = new QFile(absoluteFilePath);
        if (!csvFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(dcUniPi()) << csvFile->errorString() << absoluteFilePath;
            csvFile->deleteLater();
            return false;
        }
        QTextStream *textStream = new QTextStream(csvFile);
        while (!textStream->atEnd()) {
            QString line = textStream->readLine();
            QStringList list = line.split(',');
            if (list[4] == "Basic") {
                QString circuit = list[3].split(" ").last();
                if (list[3].contains("Digital Input", Qt::CaseSensitivity::CaseInsensitive)) {
                    m_modbusDigitalInputRegisters.insert(circuit, list[0].toInt());
                    qDebug(dcUniPi()) << "Found input register" << circuit << list[0].toInt();
                } else if (list[3].contains("Digital Output", Qt::CaseSensitivity::CaseInsensitive)) {
                    m_modbusDigitalOutputRegisters.insert(circuit, list[0].toInt());
                    qDebug(dcUniPi()) << "Found output register" << circuit << list[0].toInt();
                } else if (list[3].contains("Relay Output", Qt::CaseSensitivity::CaseInsensitive)) {
                    m_modbusDigitalOutputRegisters.insert(circuit, list[0].toInt());
                    qDebug(dcUniPi()) << "Found relay register" << circuit << list[0].toInt();
                }  else if (list[3].contains("User Programmable LED", Qt::CaseSensitivity::CaseInsensitive)) {
                    m_modbusUserLEDRegisters.insert(circuit, list[0].toInt());
                    qDebug(dcUniPi()) << "Found user programmable led" << circuit << list[0].toInt();
                }
            }
        }
        csvFile->close();
        csvFile->deleteLater();
    }

    switch(m_extensionType) {
    case ExtensionTypes::xS10:
        fileRegisterList.append(QString("/Neuron_xS10/Neuron_xS10-Registers-group-1.csv"));
        break;
    case ExtensionTypes::xS20:
        fileRegisterList.append(QString("/Neuron_xS20/Neuron_xS20-Registers-group-1.csv"));
        break;
    case ExtensionTypes::xS30:
        fileRegisterList.append(QString("/Neuron_xS30/Neuron_xS30-Registers-group-1.csv"));
        break;
    case ExtensionTypes::xS40:
        fileRegisterList.append(QString("/Neuron_xS40/Neuron_xS40-Registers-group-1.csv"));
        break;
    case ExtensionTypes::xS50:
        fileRegisterList.append(QString("/Neuron_xS50/Neuron_xS50-Registers-group-1.csv"));
        break;
    }

    foreach (QString relativeFilePath, fileRegisterList) {
        QString absoluteFilePath = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation).last() + "/nymea/modbus" + relativeFilePath;
        qDebug(dcUniPi()) << "Open CSV File:" << absoluteFilePath;
        QFile *csvFile = new QFile(absoluteFilePath);
        if (!csvFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(dcUniPi()) << csvFile->errorString() << absoluteFilePath;
            csvFile->deleteLater();
            return false;
        }
        QTextStream *textStream = new QTextStream(csvFile);
        while (!textStream->atEnd()) {
            QString line = textStream->readLine();
            QStringList list = line.split(',');
            if (list.last() == "Basic") {
                QString circuit = list[5].split(" ").last();
                if (list[5].contains("Analog Input Value", Qt::CaseSensitivity::CaseInsensitive)) {
                    m_modbusAnalogInputRegisters.insert(circuit, list[0].toInt());
                    qDebug(dcUniPi()) << "Found analog input register" << circuit << list[0].toInt();
                } else if (list[5].contains("Analog Output Value", Qt::CaseSensitivity::CaseInsensitive)) {
                    m_modbusAnalogOutputRegisters.insert(circuit, list[0].toInt());
                    qDebug(dcUniPi()) << "Found analog output register" << circuit << list[0].toInt();
                }
            }
        }
        csvFile->close();
        csvFile->deleteLater();
    }
    return true;
}


bool NeuronExtension::getDigitalInput(const QString &circuit)
{
    int modbusAddress = m_modbusDigitalInputRegisters.value(circuit);
    //qDebug(dcUniPi()) << "Reading digital input" << circuit << modbusAddress;

    if (!m_modbusInterface)
        return false;

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, modbusAddress, 1);

    if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return  false;
    }
    return true;
}


QUuid NeuronExtension::setDigitalOutput(const QString &circuit, bool value)
{
    int modbusAddress = m_modbusDigitalOutputRegisters.value(circuit);
    //qDebug(dcUniPi()) << "Setting digital ouput" << circuit << modbusAddress;

    if (!m_modbusInterface)
        return "";

    QUuid requestId = QUuid::createUuid();

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, modbusAddress, 1);
    request.setValue(0, static_cast<uint16_t>(value));

    if (QModbusReply *reply = m_modbusInterface->sendWriteRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [reply, requestId, this] {

                if (reply->error() == QModbusDevice::NoError) {
                    requestExecuted(requestId, true);
                    const QModbusDataUnit unit = reply->result();
                    int modbusAddress = unit.startAddress();
                    if(m_modbusDigitalOutputRegisters.values().contains(modbusAddress)){
                        QString circuit = m_modbusDigitalOutputRegisters.key(modbusAddress);
                        emit digitalOutputStatusChanged(circuit, unit.value(0));
                    }
                } else {
                    requestExecuted(requestId, false);
                    qCWarning(dcUniPi()) << "Read response error:" << reply->error();
                }
                reply->deleteLater();
            });
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
            return "";
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return "";
    }
    return requestId;
}

bool NeuronExtension::getDigitalOutput(const QString &circuit)
{
    int modbusAddress = m_modbusDigitalOutputRegisters.value(circuit);
    //qDebug(dcUniPi()) << "Reading digital output" << circuit << modbusAddress;

    if (!m_modbusInterface)
        return false;

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, modbusAddress, 1);

    if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return false;
    }
    return true;
}


bool NeuronExtension::getAllDigitalInputs()
{
    if (!m_modbusInterface)
        return false;

    QList<QModbusDataUnit> requests;
    QList<int> registerList = m_modbusDigitalInputRegisters.values();

    if (registerList.isEmpty()) {
        return true; //device has no digital inputs
    }

    std::sort(registerList.begin(), registerList.end());
    int previousReg = registerList.first(); //first register to read and starting point to get the following registers
    int startAddress;

    QHash<int, int> registerGroups;

    foreach (int reg, registerList) {
        //qDebug(dcUniPi()) << "Register" << reg << "previous Register" << previousReg;
        if (reg == previousReg) { //first register
            startAddress = reg;
            registerGroups.insert(startAddress, 1);
        } else if (reg == (previousReg + 1)) { //next register in block
            previousReg = reg;
            registerGroups.insert(startAddress, (registerGroups.value(startAddress) + 1)); //update block length
        } else {    // new block
            startAddress = reg;
            previousReg = reg;
            registerGroups.insert(startAddress, 1);
        }
    }

    foreach (int startAddress, registerGroups.keys()) {
        QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, startAddress, registerGroups.value(startAddress));
        if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
                QTimer::singleShot(200, reply, &QModbusReply::deleteLater);;
            } else {
                delete reply; // broadcast replies return immediately
            }
        } else {
            qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
            return false;
        }
    }
    return true;
}

bool NeuronExtension::getAllAnalogOutputs()
{
    if (!m_modbusInterface)
        return false;

    foreach (QString circuit, m_modbusAnalogOutputRegisters.keys()) {
        getAnalogOutput(circuit);
    }
    return true;
}

bool NeuronExtension::getAllAnalogInputs()
{
    if (!m_modbusInterface)
        return false;

    foreach (QString circuit, m_modbusAnalogInputRegisters.keys()) {
        getAnalogInput(circuit);
    }
    return true;
}

bool NeuronExtension::getAllDigitalOutputs()
{
    if (!m_modbusInterface)
        return false;

    QList<QModbusDataUnit> requests;
    QList<int> registerList = m_modbusDigitalOutputRegisters.values();

    if (registerList.isEmpty()) {
        return true; //device has no digital outputs
    }

    std::sort(registerList.begin(), registerList.end());
    int previousReg = registerList.first(); //first register to read and starting point to get the following registers
    int startAddress;

    QHash<int, int> registerGroups;

    foreach (int reg, registerList) {
        //qDebug(dcUniPi()) << "Register" << reg << "previous Register" << previousReg;
        if (reg == previousReg) { //first register
            startAddress = reg;
            registerGroups.insert(startAddress, 1);
        } else if (reg == (previousReg + 1)) { //next register in block
            previousReg = reg;
            registerGroups.insert(startAddress, (registerGroups.value(startAddress) + 1)); //update block length
        } else {    // new block
            startAddress = reg;
            previousReg = reg;
            registerGroups.insert(startAddress, 1);
        }
    }

    foreach (int startAddress, registerGroups.keys()) {
        QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, startAddress, registerGroups.value(startAddress));
        if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
                QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
            } else {
                delete reply; // broadcast replies return immediately
            }
        } else {
            qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
            return false;
        }
    }
    return true;
}

QUuid NeuronExtension::setAnalogOutput(const QString &circuit, double value)
{
    int modbusAddress = m_modbusAnalogOutputRegisters.value(circuit);
    if (!m_modbusInterface)
        return "";

    QUuid requestId = QUuid::createUuid();

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, modbusAddress, 2);
    request.setValue(0, static_cast<uint32_t>(value) >> 16);
    request.setValue(1, static_cast<uint32_t>(value) & 0xffff);
    //TODO cast double to 2 uint16_t

    if (QModbusReply *reply = m_modbusInterface->sendWriteRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [reply, requestId, this] {

                if (reply->error() == QModbusDevice::NoError) {
                    requestExecuted(requestId, true);
                    const QModbusDataUnit unit = reply->result();
                    int modbusAddress = unit.startAddress();
                    if(m_modbusAnalogOutputRegisters.values().contains(modbusAddress)){
                        QString circuit = m_modbusAnalogOutputRegisters.key(modbusAddress);
                        emit analogOutputStatusChanged(circuit, (unit.value(0) << 16 | unit.value(1)));
                    }
                } else {
                    requestExecuted(requestId, false);
                    qCWarning(dcUniPi()) << "Read response error:" << reply->error();
                }
                reply->deleteLater();
            });
            connect(reply, &QModbusReply::errorOccurred, this, [reply, requestId, this] (QModbusDevice::Error error){

                qCWarning(dcUniPi()) << "Modbus replay error:" << error;
                emit requestError(requestId, reply->errorString());
                reply->finished(); // To make sure it will be deleted
            });
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
            return "";
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return "";
    }
    return requestId;
}


bool NeuronExtension::getAnalogOutput(const QString &circuit)
{
    int modbusAddress = m_modbusAnalogOutputRegisters.value(circuit);

    if (!m_modbusInterface)
        return false;

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, modbusAddress, 1);

    if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return false;
    }
    return true;
}


bool NeuronExtension::getAnalogInput(const QString &circuit)
{
    int modbusAddress =  m_modbusAnalogInputRegisters.value(circuit);

    if (!m_modbusInterface)
        return false;

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::InputRegisters, modbusAddress, 1);

    if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return false;
    }
    return true;
}

QUuid NeuronExtension::setUserLED(const QString &circuit, bool value)
{
    int modbusAddress = m_modbusUserLEDRegisters.value(circuit);
    //qDebug(dcUniPi()) << "Setting digital ouput" << circuit << modbusAddress << value;

    if (!m_modbusInterface)
        return "";

    QUuid requestId = QUuid::createUuid();

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, modbusAddress, 1);
    request.setValue(0, static_cast<uint16_t>(value));

    if (QModbusReply *reply = m_modbusInterface->sendWriteRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [reply, requestId, this] {

                reply->deleteLater();
                if (reply->error() == QModbusDevice::NoError) {
                    requestExecuted(requestId, true);
                    const QModbusDataUnit unit = reply->result();
                    int modbusAddress = unit.startAddress();
                    if(m_modbusUserLEDRegisters.values().contains(modbusAddress)){
                        QString circuit = m_modbusUserLEDRegisters.key(modbusAddress);
                        emit userLEDStatusChanged(circuit, unit.value(0));
                    }
                } else {
                    requestExecuted(requestId, false);
                    qCWarning(dcUniPi()) << "Read response error:" << reply->error();
                }
            });
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
            return "";
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
        return "";
    }
    return requestId;
}


bool NeuronExtension::getUserLED(const QString &circuit)
{
    int modbusAddress = m_modbusUserLEDRegisters.value(circuit);
    //qDebug(dcUniPi()) << "Reading digital Output" << circuit << modbusAddress;

    if (!m_modbusInterface)
        return false;

    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::Coils, modbusAddress, 1);

    if (QModbusReply *reply = m_modbusInterface->sendReadRequest(request, m_slaveAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &NeuronExtension::onFinished);
            QTimer::singleShot(200, reply, &QModbusReply::deleteLater);
        } else {
            delete reply; // broadcast replies return immediately
        }
    } else {
        qCWarning(dcUniPi()) << "Read error: " << m_modbusInterface->errorString();
    }
    return true;
}


void NeuronExtension::onOutputPollingTimer()
{
    getAllDigitalOutputs();
    getAllAnalogOutputs();
}

void NeuronExtension::onInputPollingTimer()
{
    getAllDigitalInputs();
    getAllAnalogInputs();
}


void NeuronExtension::onFinished()
{
    QModbusReply *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;
    int modbusAddress = 0;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();

        for (int i = 0; i < static_cast<int>(unit.valueCount()); i++) {
            //qCDebug(dcUniPi()) << "Start Address:" << unit.startAddress() << "Register Type:" << unit.registerType() << "Value:" << unit.value(i);
            modbusAddress = unit.startAddress() + i;

            if (m_previousModbusRegisterValue.contains(modbusAddress)) {
                if (m_previousModbusRegisterValue.value(modbusAddress) == unit.value(i)) {
                    continue;
                } else  {
                    m_previousModbusRegisterValue.insert(modbusAddress, unit.value(i)); //update existing value
                }
            } else {
                m_previousModbusRegisterValue.insert(modbusAddress, unit.value(i));
            }

            QString circuit;
            switch (unit.registerType()) {
            case QModbusDataUnit::RegisterType::Coils:
                if(m_modbusDigitalInputRegisters.values().contains(modbusAddress)){
                    circuit = m_modbusDigitalInputRegisters.key(modbusAddress);
                    emit digitalInputStatusChanged(circuit, unit.value(i));
                }

                if(m_modbusDigitalOutputRegisters.values().contains(modbusAddress)){
                    circuit = m_modbusDigitalOutputRegisters.key(modbusAddress);
                    emit digitalOutputStatusChanged(circuit, unit.value(i));
                }

                if(m_modbusUserLEDRegisters.values().contains(modbusAddress)){
                    circuit = m_modbusUserLEDRegisters.key(modbusAddress);
                    emit userLEDStatusChanged(circuit, unit.value(i));
                }
                break;

            case QModbusDataUnit::RegisterType::InputRegisters:
                if(m_modbusAnalogInputRegisters.values().contains(modbusAddress)){
                    circuit = m_modbusAnalogInputRegisters.key(modbusAddress);
                    emit analogInputStatusChanged(circuit, unit.value(i));
                }
                break;
            case QModbusDataUnit::RegisterType::HoldingRegisters:
                if(m_modbusAnalogOutputRegisters.values().contains(modbusAddress)){
                    circuit = m_modbusAnalogOutputRegisters.key(modbusAddress);
                    emit analogOutputStatusChanged(circuit, unit.value(i));
                }
                break;
            case QModbusDataUnit::RegisterType::DiscreteInputs:
            case QModbusDataUnit::RegisterType::Invalid:
                qCWarning(dcUniPi()) << "Invalide register type";
                break;
            }
        }

    } else if (reply->error() == QModbusDevice::ProtocolError) {
        qCWarning(dcUniPi()) << "Read response error:" << reply->errorString() << reply->rawResult().exceptionCode();
    } else {
        qCWarning(dcUniPi()) << "Read response error:" << reply->error();
    }
    reply->deleteLater();
}
