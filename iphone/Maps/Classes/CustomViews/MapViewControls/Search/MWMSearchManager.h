#import "MWMAlertViewController.h"
#import "MWMMapDownloaderTypes.h"
#import "MWMSearchTextField.h"
#import "MWMSearchView.h"

typedef NS_ENUM(NSUInteger, MWMSearchManagerState)
{
  MWMSearchManagerStateHidden,
  MWMSearchManagerStateDefault,
  MWMSearchManagerStateTableSearch,
  MWMSearchManagerStateMapSearch
};

@interface MWMSearchManager : NSObject

@property (nullable, weak, nonatomic) IBOutlet MWMSearchTextField * searchTextField;

@property (nonatomic) MWMSearchManagerState state;

@property (nonnull, nonatomic, readonly) UIView * view;

- (nullable instancetype)init __attribute__((unavailable("init is not available")));
- (nullable instancetype)initWithParentView:(nonnull UIView *)view;

- (void)mwm_refreshUI;

- (void)searchText:(nonnull NSString *)text forInputLocale:(nullable NSString *)locale;

#pragma mark - Layout

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation
                                duration:(NSTimeInterval)duration;
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(nonnull id<UIViewControllerTransitionCoordinator>)coordinator;

@end
