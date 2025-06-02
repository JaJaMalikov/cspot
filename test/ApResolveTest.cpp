#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "LoginBlob.h"
#include "Session.h"
#include "bell/http/Reader.h"
#include "bell/http/Server.h"
#include "bell/http/Writer.h"
#include "bell/mdns/Manager.h"
#include "bell/utils/Utils.h"

#include "tao/json/forward.hpp"

TEST_CASE("MiscSpotifyApi tests", "[MiscSpotifyApi]") {
  // auto credentials = std::make_shared<cspot::SessionCredentials>("dupa");

  // // cspot::ApConnection apConnection(url);
  auto loginBlob = std::make_shared<cspot::LoginBlob>("cspottest");

  // // return;
  std::string queryStrMock =
      "action=addUser&userName=fliperspotify&blob=qGHVf5DgOFy1IepqJM1w%2Fm4Lg%"
      "2FJEOYewwOxeD8In3FKQuRi4IlBYKMWlABAd%2FkyRf86c%2FYqGSSDeFV3yY653%"
      "2FMlS15eMdSfhAGOlTVjSNtagzuy3hxVsGT3j1iqxnXtbFhGGgSjUY1bEwz0DvFcS%"
      "2BZnXn0WDVtwl5SiOi3wlU86FLXjtYkCyiruMK%"
      "2FTyJ0PMqjl52mMjEM8QeMVJyotkjuRteE6ObYmqV%2FsU7dV0oKPKtkyO%"
      "2FxmLO65ZrlxUKeycLGppC40PKBSwGUe75bR1ERUxL8RomtwXMjdKeK4FkZvnnMy%"
      "2FEaYN0wXc9NrXtpYWhWhe5FrTADrEzMvPyg1ZM7M4ybsKrIlC3V%2B5Vi2%"
      "2BUKqInoWRFUf2IZGkeBZbR1FgSkfWaRH78x9UeZhOJSFXZufQW3luMIgOB4uu0w%3D%3D&"
      "clientKey=uNsdqIF9w9mdkOBFjaSwoI%2B3mKqnOm9jqhcy%2F0KRpsxxoybb%"
      "2BBpPfj2oF%2FblwrigiIxLyNSlx89q0VrTSUTuv%"
      "2F2wOkZCtDx5nbSwHRTKZ6GhljYgW8zP1QbSjBSMDHo1&tokenType=default&loginId="
      "b4c7e370cf8ccc68573a33b0b8dc95fe&deviceName=iMac%20%28apultyna%29&"
      "deviceId=544683a1fd35eefff1ad404bcc8cc12b7b2262a5&version=2.7.1";
  loginBlob->authenticateZeroconfString(queryStrMock);

  auto session = std::make_shared<cspot::Session>(loginBlob);
  session->start();

  while (true) {
    session->runPoller();
    // bell::utils::sleepMs(1000);
  }

  // auto authenticator = std::make_shared<cspot::Authenticator>("cspottest");
  // authenticator->authenticateZeroconf(queryStrMock);
  // authenticator->connectStoredZeroconfBlob();

  // return;
  /*
  auto httpServer = std::make_shared<bell::http::Server>();

  // Get info handler
  httpServer->registerGet(
      "/spotify_handler",
      [&loginBlob](
          const std::unique_ptr<bell::http::Reader>& requestReader,
          const std::unique_ptr<bell::http::Writer>& responseWriter,
          const auto& routeParams) {
        auto queryParams = requestReader->getQueryParams().unwrap();
        std::cout << "Received get request" << std::endl;

        if (queryParams.find("action") != queryParams.end() &&
            queryParams["action"] == "getInfo") {
          auto zeroConfString = loginBlob->buildZeroconfJSONResponse();
          responseWriter->writeResponseWithBody(
              200, {{"Content-Type", "application/json"}}, zeroConfString);
        } else {
          responseWriter->writeResponseWithBody(500, {}, "Invalid action");
        }
      });

  httpServer->registerPost(
      "/spotify_handler",
      [&loginBlob](
          const std::unique_ptr<bell::http::Reader>& requestReader,
          const std::unique_ptr<bell::http::Writer>& responseWriter,
          const auto& routeParams) {
        std::cout << "Received post request" << std::endl;
        std::cout << requestReader->getBodyStringView().unwrap() << std::endl;
        auto queryParams = requestReader->getQueryParams().unwrap();
        // Authenticate the user
        for (const auto& [key, value] : queryParams) {
          std::cout << key << " = " << value << std::endl;
        }

        if (queryParams.find("action") != queryParams.end() &&
            queryParams["action"] == "addUser") {

          auto zeroConfString = loginBlob->buildZeroconfJSONResponse();
          responseWriter->writeResponseWithBody(
              200, {{"Content-Type", "application/json"}}, zeroConfString);
        } else {
          responseWriter->writeResponseWithBody(500, {}, "Invalid action");
        }
      });

  httpServer->listen(2139);
  auto service =  // Register mdns service, for spotify to find us
      bell::mdns::getDefaultManager()->advertise(
          loginBlob->getDeviceName(), "_spotify-connect._tcp", "", "",
          2139,
          {{"VERSION", "1.0"}, {"CPath", "/spotify_handler"}, {"Stack", "SP"}});

  while (true) {
    bell::utils::sleepMs(1000);
  }
  */

  // SECTION("fetchApAddresses properly fetches addresses") {
  //   // auto addresses = cspot::getApAddresses();

  //   // REQUIRE(!addresses.accessPoint.empty());
  //   // REQUIRE(!addresses.dealerAddress.empty());
  //   // REQUIRE(!addresses.spClientAddress.empty());
  // }

  // SECTION("getApAddresses properly fetches addresses") {
  //   // auto addresses = cspot::getSpotifyClientToken();

  //   // REQUIRE(!addresses.accessPoint.empty());
  //   // REQUIRE(!addresses.dealerAddress.empty());
  //   // REQUIRE(!addresses.spClientAddress.empty());
  // }
}
