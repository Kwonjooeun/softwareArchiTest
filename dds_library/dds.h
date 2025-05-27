#pragma once
#include "util\looping_thread.h"

#include <dds/pub/ddspub.hpp>
#include <dds/sub/ddssub.hpp>
#include <dds/core/ddscore.hpp>

#include <map>
#include <typeindex>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <memory>

class IHolder
{
public:
    virtual ~IHolder()
    {

    }
};

template <typename T>
class TopicHolder : public IHolder
{
public:
    TopicHolder(dds::domain::DomainParticipant& participant)
        : topic(participant, dds::topic::topic_type_name<T>().value())
    {

    }

    dds::topic::Topic<T> topic;
};


template <typename T>
class WriterHolder : public IHolder
{
public:
    WriterHolder(dds::domain::DomainParticipant& participant, dds::pub::Publisher& publisher, dds::topic::Topic<T>& topic, dds::pub::qos::DataWriterQos qos)
        : writer(publisher, topic, qos)
    {

    }

    dds::pub::DataWriter<T> writer;
};

template <typename T>
class ReaderHolder : public IHolder
{
public:
    ReaderHolder(dds::domain::DomainParticipant& participant, dds::sub::Subscriber& subscriber, dds::topic::Topic<T>& topic, dds::sub::qos::DataReaderQos qos)
        : reader(subscriber, topic, qos)
    {

    }

    dds::sub::DataReader<T> reader;
};

class DdsFacade
{
public:
    DdsFacade(int domainId, std::vector<std::string> ipList, std::string qosFileName, std::string participantQosName)
        : _qosProvider(qosFileName)
    {
        dds::domain::qos::DomainParticipantQos qos = _qosProvider.participant_qos(participantQosName);
        if (!ipList.empty())
        {
            std::string allowInterfaces = ipList[0];
            for (size_t i = 1; i < ipList.size(); ++i)
            {
                allowInterfaces += "," + ipList[i];
            }
            qos.policy<rti::core::policy::Property>().set({ "dds.transport.UDPv4.builtin.parent.allow_interfaces", allowInterfaces });
        }
        _participant = std::make_unique<dds::domain::DomainParticipant>(domainId, qos);
        _publisher = std::make_unique<dds::pub::Publisher>(*_participant);
        _subscriber = std::make_unique<dds::sub::Subscriber>(*_participant);
    }

    ~DdsFacade()
    {
        Stop();
    }

    template <typename T>
    void RegisterWriter(std::string dataWriterQosName)
    {
        if (!_topicList.contains(typeid(T)))
        {
            _topicList[typeid(T)] = std::make_unique<TopicHolder<T>>(*_participant);
        }

        if (!_writerList.contains(typeid(T)))
        {
            _writerList[typeid(T)] = std::make_unique<WriterHolder<T>>(*_participant, *_publisher, static_cast<TopicHolder<T>*>(_topicList[typeid(T)].get())->topic, 
                _qosProvider.datawriter_qos(dataWriterQosName));
        }
    }

    template <typename T, typename Callable>
    void RegisterReader(Callable callable, std::string dataReaderQosName)
    {
              if (!_topicList.contains(typeid(T)))
        {
            _topicList[typeid(T)] = std::make_unique<TopicHolder<T>>(*_participant);
        }

        if (!_readerList.contains(typeid(T)))
        {
            _readerList[typeid(T)] = std::make_unique<ReaderHolder<T>>(*_participant, *_subscriber, static_cast<TopicHolder<T>*>(_topicList[typeid(T)].get())->topic, 
                _qosProvider.datareader_qos(dataReaderQosName));
        }

        dds::sub::cond::ReadCondition read_condition
        (
            dynamic_cast<ReaderHolder<T>*>(_readerList[typeid(T)].get())->reader,
            dds::sub::status::DataState::any(),
            [=, this]()
            {
                dds::sub::LoanedSamples<T> samples = static_cast<ReaderHolder<T>*>(_readerList[typeid(T)].get())->reader->take();
                for (auto sample : samples)
                {
                    if (sample->info().valid()) {
                        callable(sample->data());
                    }
                }

            }
        );
        _waitset += read_condition;
    }

    void Start()
    {
        _loopingThread.StartLoopingThread([this]
            { 
                _waitset.dispatch(dds::core::Duration(1)); 
            });
    }

    void Stop()
    {
        _loopingThread.StopLoopingThread();
    }

    template <typename T>
    void Send(const T& message)
    {
        if (_loopingThread.IsStarted())
        {
            if (auto search = _writerList.find(typeid(T)); search != _writerList.end())
            {
                static_cast<WriterHolder<T>*>(search->second.get())->writer->write(message);
            }
        }
    }

private:
    dds::core::QosProvider _qosProvider;
    std::unique_ptr<dds::domain::DomainParticipant> _participant;
    std::unique_ptr<dds::pub::Publisher> _publisher;
    std::unique_ptr<dds::sub::Subscriber> _subscriber;
    std::map<std::type_index, std::unique_ptr<IHolder>> _topicList;
    std::map<std::type_index, std::unique_ptr<IHolder>> _writerList;
    std::map<std::type_index, std::unique_ptr<IHolder>> _readerList;
    dds::core::cond::WaitSet _waitset;
    LoopingThread _loopingThread;
};

enum class WriterMode
{
    RELIABLE,
    BEST_EFFORT,
    RELIABLE_ASYNC,
    BEST_EFFORT_ASYNC
};

enum class ReaderMode
{
    RELIABLE,
    BEST_EFFORT
};

class Dds : public DdsFacade
{
public:
    Dds(std::vector<std::string> ipList = std::vector<std::string>(), int domainId = 83)
        : DdsFacade(domainId, ipList, "a_qos.xml", "reliable")
    {

    }

    std::string GetQosName(WriterMode writerMode)
    {
        std::string result;
        switch (writerMode)
        {
        case WriterMode::RELIABLE:
            result = "reliable";
            break;
        case WriterMode::BEST_EFFORT:
            result = "bestEffort";
            break;
        case WriterMode::RELIABLE_ASYNC:
            result = "reliableAsync";
            break;
        case WriterMode::BEST_EFFORT_ASYNC:
            result = "bestEffortAsync";
            break;
        default:
            result = "reliable";
            break;
        }

        return result;
    }

    std::string GetQosName(ReaderMode readerMode)
    {
              std::string result;
        switch (readerMode)
        {
        case ReaderMode::RELIABLE:
            result = "reliable";
            break;
        case ReaderMode::BEST_EFFORT:
            result = "bestEffort";
            break;
        default:
            result = "reliable";
            break;
        }

        return result;
    }

    template <typename T>
    void RegisterWriter(WriterMode writerMode = WriterMode::RELIABLE)
    {
        DdsFacade::RegisterWriter<T>(GetQosName(writerMode));
    }

    template <typename T, typename Callable>
    void RegisterReader(Callable callable, ReaderMode readerMode = ReaderMode::RELIABLE)
    {
        DdsFacade::RegisterReader<T>(callable, GetQosName(readerMode));
    }
};
