#pragma once

#include <map>
#include <string>
#include <vector>

namespace OT {

	class Money
	{
	public:
		struct DaySummary
		{
			int income;
			int expenses;
			std::map<std::string, int> totalsByCategory;

			int net() const { return income - expenses; }
		};

		Money()
		: balance(0), todayIncome(0), todayExpenses(0), yesterdayIncome(0), yesterdayExpenses(0)
		{
		}

		void clear(int newBalance = 0)
		{
			balance = newBalance;
			todayIncome = 0;
			todayExpenses = 0;
			yesterdayIncome = 0;
			yesterdayExpenses = 0;
			todayTotalsByCategory.clear();
			yesterdayTotalsByCategory.clear();
			recentDays.clear();
		}

		void setBalance(int newBalance)
		{
			balance = newBalance;
		}

		void record(int amount, const std::string & category)
		{
			balance += amount;
			todayTotalsByCategory[category] += amount;
			if (amount >= 0)
				todayIncome += amount;
			else
				todayExpenses -= amount;
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

		int todayNet() const { return todayIncome - todayExpenses; }
		int yesterdayNet() const { return yesterdayIncome - yesterdayExpenses; }

		int recentIncome() const
		{
			int total = todayIncome;
			for (std::vector<DaySummary>::const_iterator i = recentDays.begin(); i != recentDays.end(); i++)
				total += i->income;
			return total;
		}

		int recentExpenses() const
		{
			int total = todayExpenses;
			for (std::vector<DaySummary>::const_iterator i = recentDays.begin(); i != recentDays.end(); i++)
				total += i->expenses;
			return total;
		}

		int recentNet() const { return recentIncome() - recentExpenses(); }

		static const unsigned int kRecentDayLimit = 7;

		int balance;
		int todayIncome;
		int todayExpenses;
		std::map<std::string, int> todayTotalsByCategory;

		int yesterdayIncome;
		int yesterdayExpenses;
		std::map<std::string, int> yesterdayTotalsByCategory;

		std::vector<DaySummary> recentDays;
	};
}
