#include "pch.h"
#include "KingSystemRepository.h"

#include <Ebenezer/shared/infrastructure/persistence/SqlHelpers.h>

#include <db-library/Connection.h>
#include <db-library/ConnectionManager.h>
#include <db-library/PoolConnection.h>
#include <ModelUtil/ModelUtil.h>
#include <nanodbc/nanodbc.h>
#include <spdlog/spdlog.h>

namespace Ebenezer
{

using Shared::Persistence::ExecuteSql;
using Shared::Persistence::SqlEscape;

namespace
{

bool IsValidNationIndex(int nation)
{
	return nation == KING_NATION_KARUS || nation == KING_NATION_ELMORAD;
}

} // namespace

bool KingSystemRepository::LoadAll(std::array<KingNationState, KING_NATION_COUNT>& statesOut)
{
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("KingSystemRepository::LoadAll: failed to obtain DB pool connection");
			return false;
		}

		nanodbc::statement stmt = poolConn->CreateStatement(
			"SELECT byNation, byType, strKingName, byTerritoryTariff, nTerritoryTax, "
			"nNationalTreasury FROM KING_SYSTEM");
		nanodbc::result result = nanodbc::execute(stmt);

		int loadedRows = 0;
		while (result.next())
		{
			const int nation = result.get<int>(0);
			if (!IsValidNationIndex(nation))
				continue;

			auto& s             = statesOut[nation - 1];
			s.byType            = static_cast<uint8_t>(result.get<int>(1, 0));
			s.strKingName       = result.get<nanodbc::string>(2, "");
			s.byTerritoryTariff = static_cast<uint8_t>(result.get<int>(3, 0));
			s.nTerritoryTax     = result.get<int>(4, 0);
			s.nNationalTreasury = result.get<int>(5, 0);
			++loadedRows;

			spdlog::info("KingSystemRepository::LoadAll: nation={} byType={} king='{}' tariff={} "
						 "tax={} treasury={}",
				nation, s.byType, s.strKingName, s.byTerritoryTariff, s.nTerritoryTax,
				s.nNationalTreasury);
		}

		if (loadedRows == 0)
			spdlog::warn(
				"KingSystemRepository::LoadAll: KING_SYSTEM table is empty; using defaults");
		return loadedRows > 0;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystemRepository::LoadAll: nanodbc error: {}", dbErr.what());
	}
	catch (const std::exception& ex)
	{
		spdlog::error("KingSystemRepository::LoadAll: unexpected error: {}", ex.what());
	}
	return false;
}

bool KingSystemRepository::SaveNation(int nation, const KingNationState& s)
{
	if (!IsValidNationIndex(nation))
		return false;

	const std::string nameEsc = SqlEscape(s.strKingName);

	std::string sql = "UPDATE KING_SYSTEM SET byType = ";
	sql += std::to_string(static_cast<int>(s.byType));
	sql += ", strKingName = '";
	sql += nameEsc;
	sql += "', byTerritoryTariff = ";
	sql += std::to_string(static_cast<int>(s.byTerritoryTariff));
	sql += ", nTerritoryTax = ";
	sql += std::to_string(s.nTerritoryTax);
	sql += ", nNationalTreasury = ";
	sql += std::to_string(s.nNationalTreasury);
	sql += " WHERE byNation = ";
	sql += std::to_string(nation);

	const bool ok = ExecuteSql(sql, "KingSystemRepository::SaveNation");
	if (ok)
	{
		spdlog::info("KingSystemRepository::SaveNation: persisted nation={} byType={} king='{}'",
			nation, s.byType, s.strKingName);
	}
	return ok;
}

bool KingSystemRepository::WriteCandidateNotice(
	int nation, std::string_view candidateName, std::string_view content)
{
	if (!IsValidNationIndex(nation))
		return false;
	if (candidateName.empty() || candidateName.size() > 21)
		return false;
	const size_t maxLen = 1024;
	if (content.size() > maxLen)
		content = content.substr(0, maxLen);

	const std::string nameEsc    = SqlEscape(candidateName);
	const std::string contentEsc = SqlEscape(content);

	// Upsert: delete then insert. KING_CANDIDACY_NOTICE_BOARD has no PK on
	// (strUserID, byNation), so we keep ordering simple by clearing first.
	std::string sql = "DELETE FROM KING_CANDIDACY_NOTICE_BOARD WHERE byNation = "
		+ std::to_string(nation) + " AND strUserID = '" + nameEsc + "'; "
		+ "INSERT INTO KING_CANDIDACY_NOTICE_BOARD (strUserID, byNation, sNoticeLen, strNotice) "
		+ "VALUES ('" + nameEsc + "', " + std::to_string(nation) + ", "
		+ std::to_string(content.size()) + ", CAST('" + contentEsc + "' AS VARBINARY(1024)))";
	return ExecuteSql(sql, "KingSystemRepository::WriteCandidateNotice");
}

bool KingSystemRepository::ReadCandidateNotice(
	int nation, std::string_view candidateName, std::string& contentOut)
{
	contentOut.clear();
	if (!IsValidNationIndex(nation))
		return false;

	const std::string nameEsc = SqlEscape(candidateName);
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
			return false;
		std::string sql = "SELECT CAST(strNotice AS VARCHAR(1024)) FROM "
						  "KING_CANDIDACY_NOTICE_BOARD WHERE byNation = "
			+ std::to_string(nation) + " AND strUserID = '" + nameEsc + "'";
		auto stmt   = poolConn->CreateStatement(sql);
		auto result = nanodbc::execute(stmt);
		if (result.next())
			contentOut = result.get<nanodbc::string>(0, "");
		return true;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystemRepository::ReadCandidateNotice: {}", dbErr.what());
	}
	return false;
}

} // namespace Ebenezer
