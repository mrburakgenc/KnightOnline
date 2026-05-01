#ifndef SERVER_EBENEZER_FEATURES_HOME_HOMEMODULE_H
#define SERVER_EBENEZER_FEATURES_HOME_HOMEMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Home
{

class HomeService;

// Registration entry point for the home/regene slice. Called once
// during EbenezerApp composition; binds WIZ_HOME and WIZ_REGENE on the
// router so packets dispatch into HomeService.
class HomeModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, HomeService& service);
};

} // namespace Ebenezer::Features::Home

#endif // SERVER_EBENEZER_FEATURES_HOME_HOMEMODULE_H
