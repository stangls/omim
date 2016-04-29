#import "Common.h"
#import "MWMStorage.h"

#include "Framework.h"

#include "platform/platform.hpp"

#include "storage/storage_helpers.hpp"

namespace
{
NSString * const kStorageCanShowNoWifiAlert = @"StorageCanShowNoWifiAlert";
} // namespace

using namespace storage;

@implementation MWMStorage

+ (void)startSession
{
  NSUserDefaults * ud = [NSUserDefaults standardUserDefaults];
  [ud setBool:YES forKey:kStorageCanShowNoWifiAlert];
  [ud synchronize];
}

+ (void)downloadNode:(TCountryId const &)countryId
     alertController:(MWMAlertViewController *)alertController
           onSuccess:(TMWMVoidBlock)onSuccess
{
  if (IsEnoughSpaceForDownload(countryId, GetFramework().Storage()))
  {
    [self checkConnectionAndPerformAction:[countryId, onSuccess]
    {
      GetFramework().Storage().DownloadNode(countryId);
      if (onSuccess)
        onSuccess();
    } alertController:alertController];
  }
  else
  {
    [alertController presentNotEnoughSpaceAlert];
  }
}

+ (void)retryDownloadNode:(TCountryId const &)countryId
{
  GetFramework().Storage().RetryDownloadNode(countryId);
}

+ (void)updateNode:(TCountryId const &)countryId
   alertController:(MWMAlertViewController *)alertController
{
  if (IsEnoughSpaceForUpdate(countryId, GetFramework().Storage()))
  {
    [self checkConnectionAndPerformAction:[countryId]
    {
      GetFramework().Storage().UpdateNode(countryId);
    } alertController:alertController];
  }
  else
  {
    [alertController presentNotEnoughSpaceAlert];
  }
}

+ (void)deleteNode:(TCountryId const &)countryId alertController:(MWMAlertViewController *)alertController
{
  if (GetFramework().HasUnsavedEdits(countryId))
  {
    [alertController presentUnsavedEditsAlertWithOkBlock:[countryId]
    {
      GetFramework().Storage().DeleteNode(countryId);
    }];
  }
  else
  {
    GetFramework().Storage().DeleteNode(countryId);
  }
}

+ (void)cancelDownloadNode:(TCountryId const &)countryId
{
  GetFramework().Storage().CancelDownloadNode(countryId);
}

+ (void)showNode:(TCountryId const &)countryId
{
  GetFramework().ShowNode(countryId);
}

+ (void)downloadNodes:(TCountriesVec const &)countryIds
      alertController:(MWMAlertViewController *)alertController
            onSuccess:(TMWMVoidBlock)onSuccess
{
  size_t requiredSize = accumulate(countryIds.begin(), countryIds.end(), kMaxMwmSizeBytes,
                                   [](size_t const & size, TCountryId const & countryId)
                                   {
                                     NodeAttrs nodeAttrs;
                                     GetFramework().Storage().GetNodeAttrs(countryId, nodeAttrs);
                                     return size + nodeAttrs.m_mwmSize - nodeAttrs.m_localMwmSize;
                                   });
  if (GetPlatform().GetWritableStorageStatus(requiredSize) == Platform::TStorageStatus::STORAGE_OK)
  {
    [self checkConnectionAndPerformAction:[countryIds, onSuccess]
    {
      auto & s = GetFramework().Storage();
      for (auto const & countryId : countryIds)
        s.DownloadNode(countryId);
      if (onSuccess)
        onSuccess();
    } alertController:alertController];
  }
  else
  {
    [alertController presentNotEnoughSpaceAlert];
  }
}

+ (void)checkConnectionAndPerformAction:(TMWMVoidBlock)action
                        alertController:(MWMAlertViewController *)alertController
{
  switch (Platform::ConnectionStatus())
  {
    case Platform::EConnectionType::CONNECTION_NONE:
      [alertController presentNoConnectionAlert];
      break;
    case Platform::EConnectionType::CONNECTION_WIFI:
      action();
      break;
    case Platform::EConnectionType::CONNECTION_WWAN:
    {
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kStorageCanShowNoWifiAlert])
      {
        [alertController presentNoWiFiAlertWithOkBlock:[action]
        {
          NSUserDefaults * ud = [NSUserDefaults standardUserDefaults];
          [ud setBool:NO forKey:kStorageCanShowNoWifiAlert];
          [ud synchronize];
          action();
        }];
      }
      else
      {
        action();
      }
      break;
    }
  }
}

@end
