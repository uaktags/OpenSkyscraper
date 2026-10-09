#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace OT {

	class Money
	{
	public:
		struct DaySummary
		{
			int64_t income;
			int64_t expenses;
			std::map<std::string, int64_t> totalsByCategory;

			int64_t net() const { return income - expenses; }
		};

		Money()
		: balance(0), todayIncome(0), todayExpenses(0), yesterdayIncome(0), yesterdayExpenses(0),
		  quarterStartBalance(0), lastQuarterBalance(0), quarterIncome(0), quarterExpenses(0)
		{
		}

		void clear(int64_t newBalance = 0)
		{
			balance = newBalance;
			todayIncome = 0;
			todayExpenses = 0;
			yesterdayIncome = 0;
			yesterdayExpenses = 0;
			todayTotalsByCategory.clear();
			yesterdayTotalsByCategory.clear();
			recentDays.clear();
			quarterStartBalance = newBalance;
			lastQuarterBalance = newBalance;
			quarterIncome = 0;
			quarterExpenses = 0;
			quarterTotalsByCategory.clear();
		}

		void setBalance(int64_t newBalance)
		{
			balance = newBalance;
		}

		void record(int64_t amount, const std::string & category)
		{
			balance += amount;
			todayTotalsByCategory[category] += amount;
			quarterTotalsByCategory[category] += amount;
			if (amount >= 0) {
				todayIncome += amount;
				quarterIncome += amount;
			} else {
				todayExpenses -= amount;
				quarterExpenses -= amount;
			}
		}

		void finalizeDay()
		{
			yesterdayIncome = todayIncome;
			yesterdayExpenses = todayExpenses;
			yesterdayTotalsByCategory = todayTotalsByCategory;

			DaySummary summary;
			summary.income = todayIncome;
			summary.expenses = todayExpenses;
			summary.totalsByCategory = todayTotalsByCategory;
			recentDays.push_back(summary);
			while (recentDays.size() > kRecentDayLimit)
				recentDays.erase(recentDays.begin());

			todayIncome = 0;
			todayExpenses = 0;
			todayTotalsByCategory.clear();
		}

		/// Snapshot the quarter and start a fresh one. Called by Game when
		/// the time quarter rolls over. The balance at the moment of the
		/// rollover is the "last quarter balance" shown in the finance
		/// window, and becomes the starting balance for the new quarter.
		void finalizeQuarter()
		{
			lastQuarterBalance = balance;
			quarterStartBalance = balance;
			quarterIncome = 0;
			quarterExpenses = 0;
			quarterTotalsByCategory.clear();
		}

		int64_t todayNet() const { return todayIncome - todayExpenses; }
		int64_t yesterdayNet() const { return yesterdayIncome - yesterdayExpenses; }
		int64_t quarterNet() const { return quarterIncome - quarterExpenses; }

		int64_t recentIncome() const
		{
			int64_t total = todayIncome;
			for (std::vector<DaySummary>::const_iterator i = recentDays.begin(); i != recentDays.end(); i++)
				total += i->income;
			return total;
		}

		int64_t recentExpenses() const
		{
			int64_t total = todayExpenses;
			for (std::vector<DaySummary>::const_iterator i = recentDays.begin(); i != recentDays.end(); i++)
				total += i->expenses;
			return total;
		}

		int64_t recentNet() const { return recentIncome() - recentExpenses(); }

		static const unsigned int kRecentDayLimit = 7;

		int64_t balance;
		int64_t todayIncome;
		int64_t todayExpenses;
		std::map<std::string, int64_t> todayTotalsByCategory;

		int64_t yesterdayIncome;
		int64_t yesterdayExpenses;
		std::map<std::string, int64_t> yesterdayTotalsByCategory;

		std::vector<DaySummary> recentDays;

		/// Accumulators for the current quarter (reset on quarter rollover).
		/// Read by the Finance window; quarterStartBalance is the balance at
		/// the moment the quarter began (i.e. lastQuarterBalance of the
		/// previous quarter).
		int64_t quarterStartBalance;
		int64_t lastQuarterBalance;
		int64_t quarterIncome;
		int64_t quarterExpenses;
		std::map<std::string, int64_t> quarterTotalsByCategory;
	};
}
