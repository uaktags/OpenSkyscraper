/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Wave E - EXE-shaped VIP suite guest and approval system. */
#include "VipSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Hotel.h"
#include "Item/Parking.h"
#include "JudgeSystem.h"
#include "LevelUp.h"
#include "Person.h"
#include "Time.h"
#include "XmlUtil.h"
#include "TimeWindow.h"
#include "XmlUtil.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace OT;

VipSystem::VipSystem(Game *game) : GameObject(game) { reset(); }
bool VipSystem::gameIsYoot() const { return game->activeRules.profile().yootWorld; }
void VipSystem::reset()
{
 visiting=0; lastDay=-1; watch=false; vipGuest=nullptr; favorable=false;
 vipTotal=vipTrips=result=goodReviews=badReviews=0; nextVisitTime=0;
 yootPendingVip=-1; yootArrivalTime=0; yootLastDay=-1; yootDestination=nullptr;
 yootDestType.clear(); yootDestX=yootDestY=0;}
// F8: armed from the shared daily 04:59 judge tick (Game::advanceSimTick).
void VipSystem::armDaily() { if (!gameIsYoot()) armVisit(); }
void VipSystem::clearApproval() { favorable=false; if (goodReviews>0) --goodReviews; }
// Retain the dev-key API without bypassing the calendar.
void VipSystem::scheduleNow() { armVisit(); }
bool VipSystem::liveGuest(Person *p) const { return p && game->people.count(p); }
void VipSystem::armVisit()
{
 const int day=int(std::floor(game->time.absolute));
 if (day%9!=3 || game->rating!=2 || favorable || visiting || lastDay==day) return;
 visiting=-1; lastDay=day; watch=true; vipTotal=vipTrips=result=0; vipGuest=nullptr;
 nextVisitTime=std::floor(game->time.absolute)+Time::hourToAbsolute(17);
 std::vector<Item::Hotel*> suites;
 for (auto *item : game->itemsByType["hotel_suite"]) if (auto *h=dynamic_cast<Item::Hotel*>(item))
  if (h->tenant.armed && !h->underConstruction) suites.push_back(h);
 std::sort(suites.begin(),suites.end(),[](auto *a,auto *b) {
  if (a->position.y!=b->position.y) return a->position.y<b->position.y;
  return a->position.x<b->position.x;
 });
 int floor=suites.empty()?0:suites.front()->position.y;
 game->timeWindow.showMessage("VIP reservation: Hotel Suite on floor "+std::to_string(floor));
}
void VipSystem::bindGuest(Person *p)
{
 if (gameIsYoot() || !watch || visiting!=-1 || !p || p->member!=1) return;
 auto *guest=dynamic_cast<Item::Hotel::Guest*>(p);
 if (!guest || guest->hotel->variant!=Item::Hotel::kSuite) return;
 watch=false; vipGuest=p; p->name="VIP";
 visiting=int(std::distance(game->people.begin(),game->people.find(p)))+1;
}
void VipSystem::bankTrip(Person *p,int wait)
{
 if (!gameIsYoot() && p==vipGuest && visiting>0) { vipTotal+=wait; ++vipTrips; }
}
void VipSystem::checkout(Person *p,bool normal)
{
 if (p!=vipGuest || visiting<=0) return;
 if (!normal) { voidPerson(p); return; }
 result=(vipTrips==0 ? 0 : vipTotal/vipTrips)<=150 ? 1 : 2;
 vipGuest=nullptr;
}
void VipSystem::forgetPerson(Person *p)
{
 if (p && p==vipGuest) { favorable=false; result=2; watch=false; vipGuest=nullptr; }
}
void VipSystem::voidPerson(Person *p)
{
 if (!p || p!=vipGuest) return;
 favorable=false; result=2; watch=false; vipGuest=nullptr;
}
void VipSystem::dayCheck()
{
 if (gameIsYoot()) return;
 const int day=int(std::floor(game->time.absolute));
 if (visiting && day>lastDay) {
  if (result==1) { favorable=true; game->timeWindow.showMessage("VIP approved the tower's Hotel Suite."); }
  else if (visiting!=-1) { favorable=false; game->timeWindow.showMessage("VIP was not satisfied with the tower."); }
  visiting=0; watch=false; vipGuest=nullptr; nextVisitTime=0;
  game->ratingMayIncrease();
 }
 if (day%9!=3) { watch=false; vipGuest=nullptr; }}

bool VipSystem::liveItem(Item::Item *item) const
{
	if (!item)
		return false;
	return game->items.find(item) != game->items.end();
}

Item::Item * VipSystem::findYootDestination(const std::vector<std::string> & prototypes) const
{
	for (size_t i = 0; i < prototypes.size(); ++i)
	{
		Game::ItemSetByString::const_iterator bucket = game->itemsByType.find(prototypes[i]);
		if (bucket == game->itemsByType.end())
			continue;
		for (Game::ItemSet::const_iterator it = bucket->second.begin(); it != bucket->second.end(); ++it)
			if (!(*it)->underConstruction)
				return *it;
	}
	return NULL;
}

void VipSystem::resolveSavedDestination()
{
	// The saved cell's item of the saved type. Without one the reservation
	// keeps its key (a later save still has it) and the arrival finds the
	// destination gone.
	Game::ItemSetByString::const_iterator bucket = game->itemsByType.find(yootDestType);
	if (bucket == game->itemsByType.end())
		return;
	for (Game::ItemSet::const_iterator it = bucket->second.begin(); it != bucket->second.end(); ++it)
		if ((*it)->position.x == yootDestX && (*it)->position.y == yootDestY)
		{
			yootDestination = *it;
			return;
		}
}

int VipSystem::yootVisitEstimate(Item::Item *dest) const
{
	// T2People::VIPEstimation: the VIP's CalcCurrEstimate > 200. No VIP
	// person is spawned, so use the people-estimates (P2) of the people at
	// the destination, then its 1..300 tenant estimate (J1), and only as a
	// last resort the 0..100 evaluation x 3.
	int sum = 0, n = 0;
	for (Item::Item::People::const_iterator p = dest->people.begin(); p != dest->people.end(); ++p)
	{
		sum += (*p)->yootCurrentEstimate();
		++n;
	}
	if (n > 0)
		return sum / n;
	if (dest->yootEstimate > 0)
		return dest->yootEstimate;
	return LevelUp::yootEstimateFromEvaluation(dest->getEvaluation());
}

void VipSystem::resolveYootVisit()
{
	const std::vector<RulesProfile::Vip> & vips = game->activeRules.profile().vips;
	const int index = yootPendingVip;
	Item::Item *dest = yootDestination;
	yootPendingVip = -1;
	yootDestination = NULL;
	yootDestType.clear();
	if (index < 0 || index >= (int)vips.size())
		return;
	const RulesProfile::Vip & vip = vips[index];
	if (!liveItem(dest))
	{
		++badReviews;
		game->timeWindow.showMessage("The VIP's destination is gone; the visit was cancelled.");
		return;
	}
	const int estimate = yootVisitEstimate(dest);
	if (LevelUp::yootVipSucceeds(estimate))
	{
		++goodReviews;
		game->yootVipFlags |= vip.vipFlag;
		game->timeWindow.showMessage(vip.goodMsg.empty() ? std::string("The VIP enjoyed the visit.") : vip.goodMsg);
		LOG(IMPORTANT, "YootTower VIP (grade %d) favourable: estimate %d, flag 0x%x", vip.grade, estimate, (unsigned)vip.vipFlag);
		game->ratingMayIncrease();
	}
	else
	{
		++badReviews;
		game->timeWindow.showMessage(vip.badMsg.empty() ? std::string("The VIP was not satisfied.") : vip.badMsg);
		LOG(IMPORTANT, "YootTower VIP (grade %d) unfavourable: estimate %d", vip.grade, estimate);
	}
}

void VipSystem::advanceYoot()
{
	if (yootPendingVip >= 0)
	{
		if (!yootDestination && !yootDestType.empty())
			resolveSavedDestination();
		if (game->time.absolute >= yootArrivalTime)
			resolveYootVisit();
		return;
	}

	const RulesProfile & profile = game->activeRules.profile();
	if (profile.vips.empty())
		return;
	// T2VisitVIP::Init forces cycleDay to 1 and IsBeginDay admits the first
	// call of each day; IsBeginTime matches the VIEV begin minute (10:00).
	const int day = static_cast<int>(std::floor(game->time.absolute));
	if (day == yootLastDay || game->time.hour * 60.0 < profile.vips.front().beginMinute)
		return;
	yootLastDay = day;

	const int grade = game->rating + 1;
	const int numGrades = (int)profile.grades.size();
	for (size_t i = 0; i < profile.vips.size(); ++i)
	{
		const RulesProfile::Vip & vip = profile.vips[i];
		if (vip.grade < 1 || vip.grade > numGrades)
			continue;
		const RulesProfile::Grade & row = profile.grades[vip.grade - 1];
		if (!LevelUp::yootVipMayStart(vip.grade, grade, numGrades, vip.vipFlag, row.vipFlag,
		                              row.population, game->yootVipFlags, game->population))
			continue;

		if (vip.destPrototypes.empty())
		{
			// No OpenSky counterpart for the destination plugin: like the
			// engine's "plugin not installed" branch, grant the flag outright.
			game->yootVipFlags |= vip.vipFlag;
			LOG(IMPORTANT, "YootTower VIP (grade %d) has no destination counterpart; flag 0x%x granted", vip.grade, (unsigned)vip.vipFlag);
			game->ratingMayIncrease();
			return;
		}
		Item::Item *dest = findYootDestination(vip.destPrototypes);
		if (!dest)
		{
			const std::string name = game->itemFactory.prototypesById.count(vip.destPrototypes.front())
				? game->itemFactory.prototypesById[vip.destPrototypes.front()]->name
				: vip.destPrototypes.front();
			const char c = name.empty() ? 'x' : (char)std::tolower(name[0]);
			const bool vowel = c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
			game->timeWindow.showMessage(std::string("To invite a VIP, ") + (vowel ? "an " : "a ") + name + " needs to be built.");
			return;
		}
		yootPendingVip = (int)i;
		yootDestination = dest;
		yootDestType = dest->prototype->id;
		yootDestX = dest->position.x;
		yootDestY = dest->position.y;
		yootArrivalTime = std::floor(game->time.absolute) + Time::hourToAbsolute(vip.arrivalMinute / 60.0);
		const int h = vip.arrivalMinute / 60, m = vip.arrivalMinute % 60;
		char when[32];
		std::snprintf(when, sizeof(when), "%d:%02d %s", (h % 12) ? h % 12 : 12, m, h < 12 ? "a.m." : "p.m.");
		game->timeWindow.showMessage("A VIP has made a reservation to visit your " + dest->prototype->name +
			". He will be arriving in the lobby at " + when + ".");
		LOG(IMPORTANT, "YootTower VIP (grade %d) reserved %s, arriving %s", vip.grade, dest->prototype->name.c_str(), when);
		return;
	}
}

void VipSystem::advance(double)
{
 if (gameIsYoot()) { advanceYoot(); return; }
 if (vipGuest && !liveGuest(vipGuest)) { favorable=false; result=2; watch=false; vipGuest=nullptr; }
}
void VipSystem::encodeXML(tinyxml2::XMLPrinter &xml) const
{
 xml.PushAttribute("vipVisiting",visiting); xml.PushAttribute("vipLastDay",lastDay);
 xml.PushAttribute("vipFavorable",favorable); xml.PushAttribute("vipTotal",vipTotal);
 xml.PushAttribute("vipTrips",vipTrips); xml.PushAttribute("vipResult",result);
 xml.PushAttribute("vipWatch",watch);
 if (auto *guest=dynamic_cast<Item::Hotel::Guest*>(vipGuest)) {
  xml.PushAttribute("vipHomeX",guest->hotel->position.x); xml.PushAttribute("vipHomeY",guest->hotel->position.y);
  xml.PushAttribute("vipMember",guest->member);
 }
 xml.PushAttribute("vipGoodReviews",goodReviews); xml.PushAttribute("vipBadReviews",badReviews);
 // YootTower-only state (the golden validator rejects yoot* attrs in
 // SimTower/OpenSky saves); decode defaults to -1.
 if (game->activeRules.profile().yootWorld) xml.PushAttribute("yootVipLastDay",yootLastDay);
 // YootTower-only reservation state: SimTower/OpenSky saves stay
 // byte-identical to the pre-YootTower format (decode defaults to -1).
 if (game->activeRules.profile().yootWorld && yootPendingVip>=0 && !yootDestType.empty())
 {
  xml.PushAttribute("yootVipPending",yootPendingVip);
  xml.PushAttribute("yootVipArrival",yootArrivalTime);
  xml.PushAttribute("yootVipDestType",yootDestType.c_str());
  xml.PushAttribute("yootVipDestX",yootDestX);
  xml.PushAttribute("yootVipDestY",yootDestY);
 }}
void VipSystem::decodeXML(tinyxml2::XMLElement &xml)
{
 visiting=xml.IntAttribute("vipVisiting",0); lastDay=xml.IntAttribute("vipLastDay",-1);
 favorable=xml.BoolAttribute("vipFavorable",false); vipTotal=xml.IntAttribute("vipTotal",0);
 vipTrips=xml.IntAttribute("vipTrips",0); result=xml.IntAttribute("vipResult",0);
 watch=xml.BoolAttribute("vipWatch",visiting==-1); vipGuest=nullptr;
 goodReviews=xml.IntAttribute("vipGoodReviews",0); badReviews=xml.IntAttribute("vipBadReviews",0);
 yootPendingVip=xml.IntAttribute("yootVipPending",-1); yootDestination=nullptr;
 yootArrivalTime=xml.DoubleAttribute("yootVipArrival",0.0);
 yootDestType = yootPendingVip>=0 ? std::string(xmlAttributeOr(xml,"yootVipDestType","")) : std::string();
 yootDestX=xml.IntAttribute("yootVipDestX",0); yootDestY=xml.IntAttribute("yootVipDestY",0);
 if (yootDestType.empty()) yootPendingVip=-1;
 yootLastDay=xml.IntAttribute("yootVipLastDay",-1);
 pendingMember=visiting>0 && xml.FindAttribute("vipHomeX") ? xml.IntAttribute("vipMember",1) : -1;
 pendingHomeX=xml.IntAttribute("vipHomeX",0); pendingHomeY=xml.IntAttribute("vipHomeY",0);
}
void VipSystem::relink()
{
 if (visiting>0 && pendingMember>=0) {
  for (auto *item : game->itemsByType["hotel_suite"]) if (auto *h=dynamic_cast<Item::Hotel*>(item))
   if (h->position.x==pendingHomeX && h->position.y==pendingHomeY)
    for (auto *g : h->guests) if (g->member==pendingMember) vipGuest=g;
 }
 pendingMember=-1;
 if (visiting>0 && !vipGuest && result==0) { result=2; favorable=false; watch=false; }}
