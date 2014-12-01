#include "stdinc.h"
#include "DebugManager.h"

bool DebugManager::g_isCMDDebug = false;

void DebugManager::SendCommandMessage(const string& command, DebugTask::Type type, const string& ip) noexcept
{
	fire(DebugManagerListener::DebugEvent(), DebugTask(command, type, ip)); // [!] IRainman: use real time.
}
void DebugManager::SendDetectionMessage(const string& mess) noexcept
{
	fire(DebugManagerListener::DebugEvent(), DebugTask(mess, DebugTask::DETECTION)); // [!] IRainman: use real time.
}
DebugTask::DebugTask(const string& message, Type type, const string& p_ip_and_port /*= Util::emptyString */) :
	m_message(message), m_ip_and_port(p_ip_and_port), m_time(GET_TIME()), m_type(type)
{
}
string DebugTask::format(const DebugTask& task)
{
	string out = Util::getShortTimeString(task.m_time) + ' '; // [!] IRainman fix: use real time.
	switch (task.m_type)
	{
		case DebugTask::HUB_IN:
			out += "Hub:\t[Incoming][" + task.m_ip_and_port + "]\t \t";
			break;
		case DebugTask::HUB_OUT:
			out += "Hub:\t[Outgoing][" + task.m_ip_and_port + "]\t \t";
			break;
		case DebugTask::CLIENT_IN:
			out += "Client:\t[Incoming][" + task.m_ip_and_port + "]\t \t";
			break;
		case DebugTask::CLIENT_OUT:
			out += "Client:\t[Outgoing][" + task.m_ip_and_port + "]\t \t";
			break;
#ifdef _DEBUG
		case DebugTask::DETECTION:
			break;
		default:
			dcassert(0);
#endif
	}
	out += !Text::validateUtf8(task.m_message) ? Text::toUtf8(task.m_message) : task.m_message; // [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=617
	return out;
}

