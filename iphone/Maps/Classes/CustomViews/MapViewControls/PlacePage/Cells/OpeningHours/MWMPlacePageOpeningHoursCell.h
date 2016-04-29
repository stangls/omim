#import "MWMTableViewCell.h"

@protocol MWMPlacePageOpeningHoursCellProtocol <NSObject>

- (BOOL)forcedButton;
- (BOOL)isPlaceholder;
- (BOOL)isEditor;
- (BOOL)openingHoursCellExpanded;
- (void)setOpeningHoursCellExpanded:(BOOL)openingHoursCellExpanded forCell:(UITableViewCell *)cell;

@end

@interface MWMPlacePageOpeningHoursCell : MWMTableViewCell

- (void)configWithDelegate:(id<MWMPlacePageOpeningHoursCellProtocol>)delegate
                      info:(NSString *)info;

- (CGFloat)cellHeight;

@end
