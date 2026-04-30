#include "pch.h"
#include "SqlHelpers.h"

#include <db-library/Connection.h>
#include <db-library/ConnectionManager.h>
#include <db-library/PoolConnection.h>
#include <ModelUtil/ModelUtil.h>
#include <nanodbc/nanodbc.h>
#include <spdlog/spdlog.h>

namespace Ebenezer::Shared::Persistence
{

bool ExecuteSql(std::string_view sql, std::string_view context)
{
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("SqlHelpers::ExecuteSql({}): failed to obtain DB pool connection", context);
			return false;
		}
		nanodbc::statement stmt = poolConn->CreateStatement(std::string(sql));
		nanodbc::execute(stmt);
		return true;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("SqlHelpers::ExecuteSql({}): nanodbc error: {}", context, dbErr.what());
	}
	catch (const std::exception& ex)
	{
		spdlog::error("SqlHelpers::ExecuteSql({}): unexpected error: {}", context, ex.what());
	}
	return false;
}

std::string SqlEscape(std::string_view in)
{
	std::string out;
	out.reserve(in.size() + 4);
	for (char c : in)
	{
		if (c == '\'')
			out += "''";
		else
			out += c;
	}
	return out;
}

} // namespace Ebenezer::Shared::Persistence
