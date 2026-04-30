#ifndef SERVER_EBENEZER_FEATURES_KINGSYSTEM_DOMAIN_KINGCONSTANTS_H
#define SERVER_EBENEZER_FEATURES_KINGSYSTEM_DOMAIN_KINGCONSTANTS_H

#pragma once

#include <cstdint>

namespace Ebenezer
{

// Nation IDs as used by USERDATA.Nation and KING_SYSTEM.byNation.
// (Kept in the global Ebenezer namespace for compatibility with legacy
//  call sites; promote into Features::KingSystem once the slice owns its
//  presentation layer too.)
constexpr int KING_NATION_KARUS    = 1;
constexpr int KING_NATION_ELMORAD  = 2;
constexpr int KING_NATION_COUNT    = 2;

// KING_SYSTEM.byType phase machine. Values come from the original
// KING_UPDATE_ELECTION_STATUS stored procedure semantics.
constexpr uint8_t KING_PHASE_NO_KING            = 0;
constexpr uint8_t KING_PHASE_NOMINATION_OPEN    = 1;
constexpr uint8_t KING_PHASE_NOMINATION_CLOSED  = 2;
constexpr uint8_t KING_PHASE_VOTING             = 3;
constexpr uint8_t KING_PHASE_VOTE_CLOSED        = 4;
constexpr uint8_t KING_PHASE_KING_ACTIVE        = 7;

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_KINGSYSTEM_DOMAIN_KINGCONSTANTS_H
