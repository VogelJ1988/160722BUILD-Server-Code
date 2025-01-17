#include "stdhdrs.h"

#include "Log.h"
#include "Server.h"
#include "CmdMsg.h"
#include "../ShareLib/DBCmd.h"
#include "doFunc.h"
#include "../ShareLib/packetType/ptype_server_to_server.h"
#include "../ShareLib/packetType/ptype_server_to_server_kick.h"
#include "../ShareLib/packetType/ptype_guild_battle.h"
#include "../ShareLib/LogInOutManager.h"

#include <boost/format.hpp>

void ServerSrandom(unsigned long initial_seed);
void kickUser(rnSocketIOService* service, CNetMsg::SP& msg);
void kickUserAnswer(rnSocketIOService* service, CNetMsg::SP& msg);
void kickUserByCharIndex(rnSocketIOService* service, CNetMsg::SP& msg);
void kickUserByUserIndex(rnSocketIOService* service, CNetMsg::SP& msg);
void kickUserByUserId(rnSocketIOService* service, CNetMsg::SP& msg);
void GuildBattleReg(rnSocketIOService* service, CNetMsg::SP& msg);
void GuildBattleChallenge(rnSocketIOService* service, CNetMsg::SP& msg);
bool checkGuildBattleNas(int guild_index, GoldType_t nas);
void GuildBattleStashLockOff(rnSocketIOService* service, CNetMsg::SP& msg);
void GuildBattleStashLockOffAll(rnSocketIOService* service, CNetMsg::SP& msg);

void CServer::CommandInterpreter(CDescriptor* d, CNetMsg::SP& msg)
{
	switch (msg->m_mtype)
	{
	case MSG_HELPER_REQ:
		do_Request(msg, d);
		break;

	case MSG_HELPER_REP:
		do_Reply(msg, d);
		break;

	case MSG_HELPER_COMMAND:
		do_Command(msg, d);
		break;

	case MSG_HELPER_COMMAND_STRUCT:
		do_Command_Struct(msg, d);
		break;

	case MSG_SERVER_TO_SERVER:
		{
			pTypeBase* pBase = reinterpret_cast<pTypeBase*>(msg->m_buf);
			switch (pBase->subType)
			{
			case MSG_SUB_SERVERTOSERVER_USER_LOGIN:
				LogInOutManager::Instance()->Add(d->service_, msg);
				break;

			case MSG_SUB_SERVERTOSERVER_USER_LOGOUT:
				LogInOutManager::Instance()->Delete(msg);
				break;

			case MSG_SUB_SERVERTOSERVER_CHANGE_NICK_NAME:
				{
					ServerToServerPacket::changeNickName* packet = reinterpret_cast<ServerToServerPacket::changeNickName*>(msg->m_buf);
					LogInOutManager::Instance()->changeNickName(packet->oldNickName, packet->newNickName);
				}
				break;

			case MSG_SUB_SERVERTOSERVER_KICK_REQ:
				kickUser(d->service_, msg);
				break;

			case MSG_SUB_SERVERTOSERVER_KICK_ANSER:
				kickUserAnswer(d->service_, msg);
				break;

			case MSG_SUB_SERVERTOSERVER_KICK_BY_CHAR_INDEX_REQ:
				kickUserByCharIndex(d->service_, msg);
				break;

			case MSG_SUB_SERVERTOSERVER_KICK_BY_USER_INDEX_REQ:
				kickUserByUserIndex(d->service_, msg);
				break;

			case MSG_SUB_SERVERTOSERVER_KICK_BY_USER_ID_REQ:
				kickUserByUserId(d->service_, msg);
				break;

			case MSG_SUB_GUILD_BATTLE_REG_TO_HELPER:
				GuildBattleReg(d->service_, msg);
				break;

			case MSG_SUB_GUILD_BATTLE_CHALLENGE_TO_HELPER:
				GuildBattleChallenge(d->service_, msg);
				break;

			case MSG_SUB_GUILD_BATTLE_STASH_LOCK_OFF:
				GuildBattleStashLockOff(d->service_, msg);
				break;

			case MSG_SUB_GUILD_BATTLE_STASH_LOCK_OFF_ALL:
				GuildBattleStashLockOffAll(d->service_, msg);
				break;

			default:
				LOG_ERROR("SubHelper : invalid subtye[%d]", pBase->subType);
				break;
			}
		}
		break;
	default:
		{
			unsigned char subtype = 0;
			msg->MoveFirst();
			RefMsg(msg) >> subtype;

			GAMELOG << init("WARNING", d->service_->ip().c_str())
					<< "INVALID COMMAND" << delim
					<< msg->m_mtype << delim << msg->m_size << delim
					<< (int)subtype << end;
		}

		break;
	}
}

void kickUser(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::kickUser* packet = reinterpret_cast<ServerToServerPacket::kickUser*>(msg->m_buf);
	const LogInOutManager::userInfo* pInfo = LogInOutManager::Instance()->getUserInfoByCharname(packet->kicked_charName);

	if (pInfo == NULL)
	{
		// 캐릭터를 찾지 못한 경우
		packet->subType = MSG_SUB_SERVERTOSERVER_KICK_ANSER;
		packet->result = false;
		service->deliver(msg);
		return;
	}

	pInfo->service->deliver(msg);
}

void kickUserAnswer(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::kickUser* packet = reinterpret_cast<ServerToServerPacket::kickUser*>(msg->m_buf);
	const LogInOutManager::userInfo* pInfo = LogInOutManager::Instance()->getUserInfoByCharIndex(packet->req_charIndex);
	if (pInfo == NULL)
	{
		// "kick" 요청자가 접속을 종료했을 경우
		return;
	}

	pInfo->service->deliver(msg);
}

void kickUserByCharIndex(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::kickUserByCharIndex* packet = reinterpret_cast<ServerToServerPacket::kickUserByCharIndex*>(msg->m_buf);
	const LogInOutManager::userInfo* pInfo = LogInOutManager::Instance()->getUserInfoByCharIndex(packet->kicked_charIndex);
	if (pInfo == NULL)
	{
		packet->subType = MSG_SUB_SERVERTOSERVER_KICK_BY_CHAR_INDEX_ANSWER;
		packet->result = false;
		service->deliver(msg);
		return;
	}

	{
		// 캐명으로 변경하여 처리
		CNetMsg::SP rmsg(new CNetMsg);
		ServerToServerPacket::makeKickUser(rmsg, packet->req_charIndex, pInfo->charName.c_str());
		pInfo->service->deliver(rmsg);
	}
}

void kickUserByUserIndex(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::kickUserByUserIndex* packet = reinterpret_cast<ServerToServerPacket::kickUserByUserIndex*>(msg->m_buf);
	const LogInOutManager::userInfo* pInfo = LogInOutManager::Instance()->getUserInfoByUserIndex(packet->kicked_userIndex);
	if (pInfo == NULL)
	{
		packet->subType = MSG_SUB_SERVERTOSERVER_KICK_BY_USER_INDEX_ANSWER;
		packet->result = false;
		service->deliver(msg);
		return;
	}

	{
		// 캐명으로 변경하여 처리
		CNetMsg::SP rmsg(new CNetMsg);
		ServerToServerPacket::makeKickUser(rmsg, packet->req_charIndex, pInfo->charName.c_str());
		pInfo->service->deliver(rmsg);
	}
}

void kickUserByUserId(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::kickUserByUserId* packet = reinterpret_cast<ServerToServerPacket::kickUserByUserId*>(msg->m_buf);
	const LogInOutManager::userInfo* pInfo = LogInOutManager::Instance()->getUserInfoByUserId(packet->kicked_userId);
	if (pInfo == NULL)
	{
		packet->subType = 	MSG_SUB_SERVERTOSERVER_KICK_BY_USER_ID_ANSER;
		packet->result = false;
		service->deliver(msg);
		return;
	}

	{
		// 캐명으로 변경하여 처리
		CNetMsg::SP rmsg(new CNetMsg);
		ServerToServerPacket::makeKickUser(rmsg, packet->req_charIndex, pInfo->charName.c_str());
		pInfo->service->deliver(rmsg);
	}

}

void GuildBattleReg(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::GuildBattleRegToHelper* packet = reinterpret_cast<ServerToServerPacket::GuildBattleRegToHelper*>(msg->m_buf);

	CGuild* guild = gserver.m_guildlist.findguild(packet->guild_index);

	if(guild == NULL)
	{
		packet->error_code = GUILD_BATTLE_ERROR_CONDITION;
	}
	
	else if(checkGuildBattleNas(packet->guild_index, packet->stake_nas) == false)
	{
		packet->error_code = GUILD_BATTLE_ERROR_CONDITION;
	}
	
	else
	{
		CDBCmd cmd;
		cmd.Init(&gserver.m_dbchar);
		std::string query = boost::str(boost::format("SELECT avg( c.a_level ) as ave_level FROM t_guildmember as gm, t_characters as c"
			" WHERE gm.a_char_index = c.a_index AND gm.a_guild_index = %d") % packet->guild_index);
		cmd.SetQuery(query);
		cmd.Open();

		while(cmd.MoveNext())
		{
			cmd.GetRec("ave_level", packet->ave_level);
		}

		packet->error_code = GUILD_BATTLE_SUCCESS_REGIST;
	}

	service->deliver(msg);

	//길드창고 사용 못하도록 처리
	if(packet->error_code == GUILD_BATTLE_SUCCESS_REGIST)
		guild->m_isUseTheStashAndSkill = false;
}

void GuildBattleChallenge(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::GuildBattleChallengeToHelper* packet = reinterpret_cast<ServerToServerPacket::GuildBattleChallengeToHelper*>(msg->m_buf);

	CGuild* guild = gserver.m_guildlist.findguild(packet->guild_index);

	if(guild == NULL)
		packet->error_code = GUILD_BATTLE_ERROR_CONDITION;
	
	else if(checkGuildBattleNas(packet->guild_index, packet->stake_nas) == false)
		packet->error_code = GUILD_BATTLE_ERROR_CONDITION;
	
	else
		packet->error_code = GUILD_BATTLE_SUCCESS_CHALLENGE;

	service->deliver(msg);

	//길드창고 사용 못하도록 처리
	if(packet->error_code == GUILD_BATTLE_SUCCESS_CHALLENGE)
		guild->m_isUseTheStashAndSkill = false;
}

void GuildBattleStashLockOff(rnSocketIOService* service, CNetMsg::SP& msg)
{
	ServerToServerPacket::GuildBattleStashLockOff* packet = reinterpret_cast<ServerToServerPacket::GuildBattleStashLockOff*>(msg->m_buf);
	
	CGuild* guild = gserver.m_guildlist.findguild(packet->guild_index);

	if(guild == NULL)
		return;

	guild->m_isUseTheStashAndSkill = true;
}

void GuildBattleStashLockOffAll(rnSocketIOService* service, CNetMsg::SP& msg)
{
	CGuild* ret = gserver.m_guildlist.head();
	while (ret)
	{
		ret->m_isUseTheStashAndSkill = true;
		ret = ret->nextguild();
	}
}

bool checkGuildBattleNas(int guild_index, GoldType_t nas)
{
	GoldType_t guild_nas = 0;

	CDBCmd cmd;
	cmd.Init(&gserver.m_dbchar);

	std::string query = boost::str(boost::format("select * from t_guild_stash_info where a_guild_idx = %d") % guild_index);
	cmd.SetQuery(query);
	if( cmd.Open() == false)
	{
		return false;
	}

	while( cmd.MoveNext() )
	{
		cmd.GetRec("a_nas", guild_nas);
	}
	
	cmd.Close();

	if(guild_nas < nas)
		return false;

	return true;
}

