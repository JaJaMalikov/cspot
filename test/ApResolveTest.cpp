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

  // return;

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

  return;
  /*
  auto httpServer = std::make_shared<bell::http::Server>();

  // Get info handler
  httpServer->registerGet(
      "/spotify_handler",
      [&sessionCredentials](
          const std::unique_ptr<bell::http::Reader>& requestReader,
          const std::unique_ptr<bell::http::Writer>& responseWriter,
          const auto& routeParams) {
        auto queryParams = requestReader->getQueryParams().unwrap();
        std::cout << "Received get request" << std::endl;

        if (queryParams.find("action") != queryParams.end() &&
            queryParams["action"] == "getInfo") {
          auto zeroConfString = sessionCredentials->buildZeroconfJSONResponse();
          responseWriter->writeResponseWithBody(
              200, {{"Content-Type", "application/json"}}, zeroConfString);
        } else {
          responseWriter->writeResponseWithBody(500, {}, "Invalid action");
        }
      });

  httpServer->registerPost(
      "/spotify_handler",
      [&sessionCredentials](
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

          auto zeroConfString = sessionCredentials->buildZeroconfJSONResponse();
          responseWriter->writeResponseWithBody(
              200, {{"Content-Type", "application/json"}}, zeroConfString);
        } else {
          responseWriter->writeResponseWithBody(500, {}, "Invalid action");
        }
      });

  httpServer->listen(2139);
  auto service =  // Register mdns service, for spotify to find us
      bell::mdns::getDefaultManager()->advertise(
          sessionCredentials->getDeviceName(), "_spotify-connect._tcp", "", "",
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
