#include "stdhdrs.h"

#include "Log.h"
#include "Character.h"
#include "Server.h"
#include "../ShareLib/packetType/ptype_guild_battle.h"
#include "../ShareLib/packetType/ptype_server_to_server.h"
#include "GuildBattleManager.h"
#include "CmdMsg.h"

void do_GuildBattle_Reg(CPC* pc, CNetMsg::SP& msg);
void do_GuildBattle_Challenge(CPC* pc, CNetMsg::SP& msg);
void do_GuildBattle_Cancel(CPC* pc, CNetMsg::SP& msg);
void do_GuildBattle_List(CPC* pc, CNetMsg::SP& msg);
void do_GuildBattle_Agree(CPC* pc, CNetMsg::SP& msg);
bool CheckInvalidGuildInfo(CPC* pc);


void do_Guild_Battle_Extend(CPC* pc, CNetMsg::SP& msg)
{
	pTypeBase* pBase = reinterpret_cast<pTypeBase*>(msg->m_buf);

	switch(pBase->subType)
	{
	case MSG_SUB_GUILD_BATTLE_REGIST:
		do_GuildBattle_Reg(pc, msg);
		break;
	case MSG_SUB_GUILD_BATTLE_CHALLENGE:
		do_GuildBattle_Challenge(pc, msg);
		break;
	case MSG_SUB_GUILD_BATTLE_CANCEL:
		do_GuildBattle_Cancel(pc, msg);
		break;
	case MSG_SUB_GUILD_BATTLE_LIST:
		do_GuildBattle_List(pc, msg);
		break;
	case MSG_SUB_GUILD_BATTLE_CHALLENGE_AGREE:
		do_GuildBattle_Agree(pc, msg);
		break;
	}
}

void do_GuildBattle_Reg(CPC* pc, CNetMsg::SP& msg)
{
	if( CheckInvalidGuildInfo(pc) == false)
		return ;

	if(gserver->m_helper->isRun() == false)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	CGuild* guild = pc->m_guildInfo->guild();

	if(pc->m_guildInfo->guild()->battleState() != GUILD_BATTLE_STATE_PEACE)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		GuildBattleErrMsg(rmsg, MSG_GUILD_ERROR_ALREADY_BATTLE);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	if(guild->level() < 6)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	CSkill* skill = guild->m_passiveSkillList.Find(1377 /*길드창고스킬*/);
	if(skill == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	RequestClient::GuildBattleRegist* packet = reinterpret_cast<RequestClient::GuildBattleRegist*>(msg->m_buf);

	if(packet->stake_nas < 1000000 || packet->stake_nas > 1000000000)
	{
		//HACKING?
		LOG_ERROR("HACKING? Invalid nas data. char_index[%d], nas[%d]", pc->m_index, packet->stake_nas);
		pc->m_desc->Close("Invalid nas data");
		return;
	}
	if(packet->stake_gp < 1000 || packet->stake_gp > 1000000)
	{
		//HACKING?
		LOG_ERROR("HACKING? Invalid guildpoint data. char_index[%d], guild_point[%d]", pc->m_index, packet->stake_gp);
		pc->m_desc->Close("Invalid guildpoint data");
		return;
	}

	if(guild->GetGuildPoint() < packet->stake_gp)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	if (packet->zone_index    != ZONE_START			&& packet->zone_index != ZONE_DRATAN
		&& packet->zone_index != ZONE_MERAC			&& packet->zone_index != ZONE_EGEHA
		&& packet->zone_index != ZONE_STREIANA		&& packet->zone_index != ZONE_MONDSHINE
		&& packet->zone_index != ZONE_TARIAN		&& packet->zone_index != ZONE_BLOODYMIR
		&& packet->zone_index != ZONE_PK_TOURNAMENT)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		GuildBattleErrMsg(rmsg, MSG_GUILD_ERROR_BATTLE_ZONE);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	{
		CNetMsg::SP rmsg(new CNetMsg);
		ServerToServerPacket::makeGuildBattleRegToHelperMsg(rmsg, guild->index(), pc->m_index, packet->stake_nas, packet->stake_gp, packet->guild_battle_time, packet->zone_index);
		SEND_Q(rmsg, gserver->m_helper);
	}
}

void do_GuildBattle_Challenge(CPC* pc, CNetMsg::SP& msg)
{
	if( CheckInvalidGuildInfo(pc) == false)
		return ;

	if(pc->m_guildInfo->guild()->level() < 6)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	RequestClient::GuildBattleChallenge* packet = reinterpret_cast<RequestClient::GuildBattleChallenge*>(msg->m_buf);
		
	CGuild* target_guild = gserver->m_guildlist.findguild(packet->guild_index);
	if(target_guild == NULL)
	{
		LOG_ERROR("HACKING? char_index[%d], guild_index[%d]", pc->m_index, packet->guild_index);
		pc->m_desc->Close("INVALID GUILD INFO");
		return;
	}

	CSkill* skill = pc->m_guildInfo->guild()->m_passiveSkillList.Find(1377 /*길드창고스킬*/);
	if(skill == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	CPC* ch = target_guild->boss()->GetPC();
	if(ch == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_AREADY);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	if(GuildBattleManager::instance()->check_banish(ch->m_guildInfo->guild()->index(), pc->m_guildInfo->guild()->index()) == false)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_FAIL_CHALLENGE);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	//타겟길드
	st_multi_index* data1 = GuildBattleManager::instance()->find(packet->guild_index);

	if(data1 == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_AREADY);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	//나의길드가 길드전투 리스트에 등록되어 있는 중이라면 
	st_multi_index* data2 = GuildBattleManager::instance()->find(pc->m_guildInfo->guild()->index());

	if(data2 != NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_AREADY);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	if(pc->m_guildInfo->guild()->GetGuildPoint() < data1->stake_gp)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	//헬퍼로 메시지 전달. 조건이 맞는지 확인

	{
		CNetMsg::SP rmsg(new CNetMsg);
		ServerToServerPacket::makeGuildBattleChallengeToHelperMsg(rmsg, pc->m_guildInfo->guild()->index(), data1->guild_index, ch->m_index, pc->m_index, data1->stake_nas);
		SEND_Q(rmsg, gserver->m_helper);
	}
}

void do_GuildBattle_Cancel(CPC* pc, CNetMsg::SP& msg)
{
	if( CheckInvalidGuildInfo(pc) == false)
		return ;

	RequestClient::GuildBattleRegCancel* packet = reinterpret_cast<RequestClient::GuildBattleRegCancel*>(msg->m_buf);

	GuildBattleManager::instance()->cancel(pc->m_guildInfo->guild()->index());

	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_SUCCESS_CANCEL);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}
}

void do_GuildBattle_List(CPC* pc, CNetMsg::SP& msg)
{
	if( CheckInvalidGuildInfo(pc) == false)
		return ;

	RequestClient::GuildBattleList* packet = reinterpret_cast<RequestClient::GuildBattleList*>(msg->m_buf);
	
	GuildBattleManager::instance()->SendBattleList(pc, packet->page_num, packet->guild_battle_time);
}

void do_GuildBattle_Agree(CPC* pc, CNetMsg::SP& msg)
{
	if( CheckInvalidGuildInfo(pc) == false)
		return ;

	RequestClient::GuildBattleChallengeAgree* packet = reinterpret_cast<RequestClient::GuildBattleChallengeAgree*>(msg->m_buf);
	
	CPC* ch = PCManager::instance()->getPlayerByCharIndex(packet->char_index);

	if(ch == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_AREADY);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	st_multi_index* data = GuildBattleManager::instance()->find(pc->m_guildInfo->guild()->index());
	if(data == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_AREADY);
		SEND_Q(rmsg, pc->m_desc);
		return;
	}

	if(packet->isAgree == true)
	{
		//수락 메시지
		CNetMsg::SP rmsg(new CNetMsg);
		MakeGuildBattleChallengeAgreeMsgRes(rmsg, pc->m_guildInfo->guild()->name(), GUILD_BATTLE_SUCCESS_AGREE, data->zone_index);
		SEND_Q(rmsg, ch->m_desc);

		//길드 전투 시작

		if (gserver->isRunHelper())
		{
			st_multi_index* data = GuildBattleManager::instance()->find(pc->m_guildInfo->guild()->index());
			if(data == NULL)
			{
				CNetMsg::SP rmsg(new CNetMsg);
				makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_AREADY);
				SEND_Q(rmsg, pc->m_desc);
				return;
			}

			CNetMsg::SP rmsg(new CNetMsg);
			HelperGuildBattleReqMsg(rmsg, pc->m_guildInfo->guild()->index(), ch->m_guildInfo->guild()->index(), data->stake_nas, data->stake_gp, data->zone_index, (data->battle_time * 600 + GUILD_BATTLE_WAIT_TIME));
			SEND_Q(rmsg, gserver->m_helper);

			//등록리스트에서 제거
			GuildBattleManager::instance()->cancel(ch->m_guildInfo->guild()->index());
			GuildBattleManager::instance()->cancel(pc->m_guildInfo->guild()->index());
		}
	}
	else
	{
		//도전 거부당함 리스트에 저장 15분동안 처리 불가
		GuildBattleManager::instance()->push_banish(pc->m_guildInfo->guild()->index(), ch->m_guildInfo->guild()->index());

		//거부 메시지 전송
		CNetMsg::SP rmsg(new CNetMsg);
		MakeGuildBattleChallengeAgreeMsgRes(rmsg, pc->m_guildInfo->guild()->name(), GUILD_BATTLE_SUCCESS_REFUSE, -1);
		SEND_Q(rmsg, ch->m_desc);
		return;
		
		//해당길드 헬퍼에 락off메시지 전송
	}
}

//////////////////////////////////////////////////////////////////////////
bool CheckInvalidGuildInfo(CPC* pc)
{
	if(pc->m_guildInfo == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return false;
	}

	if(pc->m_guildInfo->guild() == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return false;
	}

	CGuild* guild = pc->m_guildInfo->guild();

	if(guild->boss()->GetPC() == NULL)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION);
		SEND_Q(rmsg, pc->m_desc);
		return false;
	}

	if(guild->boss()->GetPC() != pc)
	{
		CNetMsg::SP rmsg(new CNetMsg);
		makeGuildBattleErrorMsg(rmsg, GUILD_BATTLE_ERROR_CONDITION_GUILD_MASTER);
		SEND_Q(rmsg, pc->m_desc);
		return false;
	}

	return true;
}