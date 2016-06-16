#import "MWMPlacePage.h"
#import "MWMPlacePageButtonCell.h"
#import "Statistics.h"

#import "UIColor+MapsMeColor.h"

@interface MWMPlacePageButtonCell ()

@property (weak, nonatomic) MWMPlacePage * placePage;
@property (weak, nonatomic) IBOutlet UIButton * titleButton;
@property (nonatomic) MWMPlacePageCellType type;

@end

@implementation MWMPlacePageButtonCell

- (void)config:(MWMPlacePage *)placePage forType:(MWMPlacePageCellType)type
{
  self.placePage = placePage;
  switch (type)
  {
  case MWMPlacePageCellTypeAddBusinessButton:
    [self.titleButton setTitle:L(@"placepage_add_business_button") forState:UIControlStateNormal];
    break;
  case MWMPlacePageCellTypeEditButton:
    [self.titleButton setTitle:L(@"edit_place") forState:UIControlStateNormal];
    break;
  case MWMPlacePageCellTypeAddPlaceButton:
    [self.titleButton setTitle:L(@"placepage_add_place_button") forState:UIControlStateNormal];
    break;
  default:
    NSAssert(false, @"Invalid place page cell type!");
    break;
  }
  self.type = type;
}

- (IBAction)buttonTap
{
  switch (self.type)
  {
  case MWMPlacePageCellTypeEditButton:
    [Statistics logEvent:kStatEventName(kStatPlacePage, kStatEdit)];
    [self.placePage editPlace];
    break;
  case MWMPlacePageCellTypeAddBusinessButton:
    [Statistics logEvent:kStatEditorAddClick withParameters:@{kStatValue : kStatPlacePage}];
    [self.placePage addBusiness];
    break;
  case MWMPlacePageCellTypeAddPlaceButton:
    [Statistics logEvent:kStatEditorAddClick withParameters:@{kStatValue : kStatPlacePageNonBuilding}];
    [self.placePage addPlace];
    break;
  default:
    NSAssert(false, @"Incorrect cell type!");
    break;
  }
}

@end
