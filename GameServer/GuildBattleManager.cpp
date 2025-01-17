#include "stdhdrs.h"
#include "Server.h"
#include "Character.h"
#include "Guild.h"
#include "GuildBattleManager.h"
#include "../ShareLib/packetType/ptype_guild_battle.h"
#include "CmdMsg.h"

GuildBattleManager::GuildBattleManager()
{
	m_fee_nas = 10;
	m_fee_gp = 10;
}

GuildBattleManager::~GuildBattleManager()
{
}

GuildBattleManager* GuildBattleManager::instance()
{
	static GuildBattleManager m_instance;
	return &m_instance;
}

void GuildBattleManager::regist( CGuild* guild, GoldType_t stake_nas, int guild_point, int battle_time, int ave_level, int zone_index )
{
	if(guild == NULL)
		return;

	st_multi_index* data = new st_multi_index;
	data->guild_index = guild->index();
	data->stake_nas = stake_nas;
	data->stake_gp = guild_point;
	data->battle_time = battle_time;
	data->guild = guild;
	data->ave_level = ave_level;
	data->zone_index = zone_index;

	m_gb_multi_index.insert(data);
}

bool GuildBattleManager::cancel( int guild_index )
{
	GM_MULTI_INDEX::nth_index<0>::type& use_index0 = m_gb_multi_index.get<0>();
	GM_MULTI_INDEX::nth_index<0>::type::iterator iterfind = use_index0.find(guild_index);
	if(iterfind != use_index0.end())
	{
		delete (*iterfind);
		m_gb_multi_index.erase(iterfind);

		return true;
	}

	return false;
}

void GuildBattleManager::setFeeNas( int fee )
{
	m_fee_nas = fee;
}

int GuildBattleManager::getFeeNas( int fee )
{
	return m_fee_nas;
}

void GuildBattleManager::setFeeGp( int fee )
{
	m_fee_gp = fee;
}

int GuildBattleManager::getFeeGp( int fee )
{
	return m_fee_gp;
}

st_multi_index* GuildBattleManager::find(int guild_index)
{
	const GM_MULTI_INDEX::nth_index<0>::type& use_index = m_gb_multi_index.get<0>();
	const GM_MULTI_INDEX::nth_index<0>::type::iterator iterfind = use_index.find(guild_index);

	if(iterfind != use_index.end())
		return *iterfind;

	return NULL;
}

void GuildBattleManager::SendBattleList( CPC* ch, int page, int battle_time)
{
	CNetMsg::SP rmsg(new CNetMsg);
	ResponseClient::GuildBattleList* p  = reinterpret_cast<ResponseClient::GuildBattleList*>(rmsg->m_buf);
	p->type = MSG_GUILD_BATTLE;
	p->subType = MSG_SUB_GUILD_BATTLE_LIST;
	int count = 0;

	st_multi_index* data = this->find(ch->m_guildInfo->guild()->index());
	if(data != NULL)
		p->isReg = true;
	else
		p->isReg = false;

	if(battle_time == 0)
	{
		//필터링 없이 최근 등록순으로
		const GM_MULTI_INDEX::nth_index<0>::type& use_index = m_gb_multi_index.get<0>();
		GM_MULTI_INDEX::reverse_iterator it = m_gb_multi_index.rbegin();
		GM_MULTI_INDEX::reverse_iterator it_end = m_gb_multi_index.rend();

		for(int i = 0; i < (page * ONE_PAGE_MAX_COUNT); i++)
		{
			if(it == it_end)
				return;

			it++;
		}
		
		for(; it != it_end; it++, count++)
		{
			if(count == ONE_PAGE_MAX_COUNT)
			{
				p->isNext = true;
				break;
			}

			p->data[count].guild_battle_count = (*it)->guild->m_battle_total_count;
			p->data[count].guild_battle_time = (*it)->battle_time;
			p->data[count].guild_index = (*it)->guild->index();
			p->data[count].guild_member_count = (*it)->guild->membercount();
			p->data[count].guild_member_level_aver = (*it)->ave_level;
			strncpy(p->data[count].guild_master_name, (*it)->guild->boss()->GetName(), MAX_CHAR_NAME_LENGTH);
			strncpy(p->data[count].guild_name, (*it)->guild->name(), MAX_GUILD_NAME_LENGTH);
			p->data[count].guild_win_count = (*it)->guild->m_battle_win_count;
			p->data[count].stake_gp = (*it)->stake_gp;
			p->data[count].stake_nas = (*it)->stake_nas;
			p->data[count].zone_index = (*it)->zone_index;
		}
	}
	else
	{
		const GM_MULTI_INDEX::nth_index<1>::type& use_index = m_gb_multi_index.get<1>();
		GM_MULTI_INDEX::nth_index<1>::type::iterator it_upper = use_index.upper_bound(battle_time);
		GM_MULTI_INDEX::nth_index<1>::type::iterator it_lower = use_index.lower_bound(battle_time);

		if(it_upper != it_lower)
		{
			for(int i = 0; i < (page * ONE_PAGE_MAX_COUNT); i++)
			{
				it_upper--;

				if(it_upper == it_lower)
				{
					//HACKING?
					LOG_ERROR("HACKING? Invalid PageNum. char_index[%d], page_num[%d]", ch->m_index, page);
					ch->m_desc->Close("Invalid PageNum");
					return;
				}
			}

			do 
			{
				it_upper--;

				if(count == ONE_PAGE_MAX_COUNT)
				{
					p->isNext = true;
					break;
				}

				p->data[count].guild_battle_count = (*it_upper)->guild->m_battle_total_count;
				p->data[count].guild_battle_time = (*it_upper)->battle_time;
				p->data[count].guild_index = (*it_upper)->guild->index();
				p->data[count].guild_member_count = (*it_upper)->guild->membercount();
				p->data[count].guild_member_level_aver = (*it_upper)->ave_level;
				strncpy(p->data[count].guild_master_name, (*it_upper)->guild->boss()->GetName(), MAX_CHAR_NAME_LENGTH);
				strncpy(p->data[count].guild_name, (*it_upper)->guild->name(), MAX_GUILD_NAME_LENGTH);
				p->data[count].guild_win_count = (*it_upper)->guild->m_battle_win_count;
				p->data[count].stake_gp = (*it_upper)->stake_gp;
				p->data[count].stake_nas = (*it_upper)->stake_nas;
				p->data[count].zone_index = (*it_upper)->zone_index;

				count++;

			} while (it_lower != it_upper);
		}
	}
	
	p->count = count;
	rmsg->setSize(sizeof(ResponseClient::GuildBattleList) + (sizeof(ResponseClient::GuildBattleData) * count));
	SEND_Q(rmsg, ch->m_desc);
}

void GuildBattleManager::push_banish( int my_guild_index, int target_guild_index )
{
	std::map< int, std::map<int, int> >::iterator it = _map_banish.find(my_guild_index);
	if( it == _map_banish.end() )
	{
		std::map<int, int> _map;
		_map.insert(std::map<int, int>::value_type(target_guild_index, gserver->m_nowseconds));

		_map_banish.insert(std::map< int, std::map<int, int> >::value_type(my_guild_index, _map));
	}
	else
	{
		it->second.insert(std::map<int, int>::value_type(target_guild_index, gserver->m_nowseconds));
	}
}

bool GuildBattleManager::check_banish( int my_guild_index, int target_guild_index )
{
	std::map< int, std::map<int, int> >::iterator it = _map_banish.find(my_guild_index);
	if( it == _map_banish.end() )
	{
		return true;
	}
	else
	{
		std::map<int, int>::iterator second_it = it->second.find(target_guild_index);

		if(second_it == it->second.end())
		{
			return true;
		}
		else
		{
			//데이터가 있다면 15분 체크
			if(second_it->second < gserver->m_nowseconds - (60 * 60 * 15) )
			{
				//데이터 지우고 리턴
				it->second.erase(second_it);
				return true;
			}
			else
			{
				//길드 전투 불가능
				return false;
			}
		}
	}
}

void GuildBattleManager::delete_banish( int my_guild_index )
{
	//로그아웃 시 처리
	_map_banish.erase(my_guild_index);
}

void GuildBattleManager::giveup( int char_index )
{
	CPC* pc = PCManager::instance()->getPlayerByCharIndex(char_index);
	
	CGuild* giveup_guild = NULL;

	if(pc->m_guildInfo != NULL)
		giveup_guild = pc->m_guildInfo->guild();

	if(giveup_guild == NULL)
		return;

	if(giveup_guild->battleState() != GUILD_BATTLE_STATE_ING)
		return;
	
	giveup_guild->KillCount(0);

	if(gserver->isRunHelper())
	{
		CNetMsg::SP rmsg(new CNetMsg);
		HelperGuildBattleGiveUpReqMsg(rmsg, giveup_guild->index(), giveup_guild->battleIndex());
		SEND_Q(rmsg, gserver->m_helper);
	}
}
