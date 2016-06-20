#import "Common.h"
#import "MWMCircularProgress.h"
#import "MWMNavigationDashboardEntity.h"
#import "MWMNavigationDashboardManager.h"
#import "MWMRoutePointCell.h"
#import "MWMRoutePointLayout.h"
#import "MWMRoutePreview.h"
#import "Statistics.h"
#import "TimeUtils.h"
#import "UIColor+MapsMeColor.h"
#import "UIFont+MapsMeFonts.h"

static CGFloat const kAdditionalHeight = 20.;

@interface MWMRoutePreview () <MWMRoutePointCellDelegate, MWMCircularProgressProtocol>

@property (weak, nonatomic) IBOutlet UIView * pedestrian;
@property (weak, nonatomic) IBOutlet UIView * vehicle;
@property (weak, nonatomic) IBOutlet UIView * bicycle;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * planningRouteViewHeight;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * planningContainerHeight;
@property (weak, nonatomic, readwrite) IBOutlet UIButton * extendButton;
@property (weak, nonatomic) IBOutlet UIButton * goButton;
@property (weak, nonatomic) IBOutlet UICollectionView * collectionView;
@property (weak, nonatomic) IBOutlet MWMRoutePointLayout * layout;
@property (weak, nonatomic) IBOutlet UIImageView * arrowImageView;
@property (weak, nonatomic) IBOutlet UIView * statusBox;
@property (weak, nonatomic) IBOutlet UIView * planningBox;
@property (weak, nonatomic) IBOutlet UIView * resultsBox;
@property (weak, nonatomic) IBOutlet UIView * errorBox;
@property (weak, nonatomic) IBOutlet UILabel * resultLabel;
@property (weak, nonatomic) IBOutlet UILabel * arriveLabel;
@property (weak, nonatomic) IBOutlet UIImageView * completeImageView;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * statusBoxHeight;
@property (nonatomic) UIImageView * movingCellImage;

@property (nonatomic) BOOL isNeedToMove;
@property (nonatomic) NSIndexPath * indexPathOfMovingCell;

@end

@implementation MWMRoutePreview
{
  map<routing::RouterType, MWMCircularProgress *> m_progresses;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  self.layer.shouldRasterize = YES;
  self.layer.rasterizationScale = UIScreen.mainScreen.scale;
  [self.collectionView registerNib:[UINib nibWithNibName:[MWMRoutePointCell className] bundle:nil]
        forCellWithReuseIdentifier:[MWMRoutePointCell className]];
  [self setupProgresses];
}

- (void)setupProgresses
{
  [self addProgress:self.vehicle imageName:@"ic_drive" routerType:routing::RouterType::Vehicle];
  [self addProgress:self.pedestrian imageName:@"ic_walk" routerType:routing::RouterType::Pedestrian];
  [self addProgress:self.bicycle imageName:@"ic_bike_route" routerType:routing::RouterType::Bicycle];
}

- (void)addProgress:(UIView *)parentView imageName:(NSString *)imageName routerType:(routing::RouterType)routerType
{
  MWMCircularProgress * progress = [[MWMCircularProgress alloc] initWithParentView:parentView];
  [progress
   setImage:[UIImage imageNamed:imageName]
   forStates:{MWMCircularProgressStateNormal, MWMCircularProgressStateFailed,
     MWMCircularProgressStateSelected, MWMCircularProgressStateProgress,
     MWMCircularProgressStateSpinner, MWMCircularProgressStateCompleted}];
  progress.delegate = self;
  m_progresses[routerType] = progress;
}

- (void)didMoveToSuperview
{
  [self setupActualHeight];
}

- (void)addToView:(UIView *)superview
{
  [super addToView:superview];
  [superview bringSubviewToFront:superview];
}

- (void)configureWithEntity:(MWMNavigationDashboardEntity *)entity
{
  NSString * eta = [NSDateFormatter estimatedArrivalTimeWithSeconds:@(entity.timeToTarget)];
  NSString * resultString = [NSString stringWithFormat:@"%@ • %@ %@",
                             eta,
                             entity.targetDistance,
                             entity.targetUnits];
  NSMutableAttributedString * result = [[NSMutableAttributedString alloc] initWithString:resultString];
  [result addAttributes:self.etaAttributes range:NSMakeRange(0, eta.length)];
  self.resultLabel.attributedText = result;
  if (!IPAD)
    return;

  NSString * arriveStr = [NSDateFormatter localizedStringFromDate:[[NSDate date]
                                                                   dateByAddingTimeInterval:entity.timeToTarget]
                                                        dateStyle:NSDateFormatterNoStyle
                                                        timeStyle:NSDateFormatterShortStyle];
  self.arriveLabel.text = [NSString stringWithFormat:L(@"routing_arrive"), arriveStr.UTF8String];
}

- (void)statePrepare
{
  for (auto const & progress : m_progresses)
    progress.second.state = MWMCircularProgressStateNormal;
  self.arrowImageView.transform = CGAffineTransformMakeRotation(M_PI);
  self.goButton.hidden = NO;
  self.goButton.enabled = NO;
  self.extendButton.selected = YES;
  [self setupActualHeight];
  [self.collectionView reloadData];
  [self.layout invalidateLayout];
  self.statusBox.hidden = YES;
  self.resultsBox.hidden = YES;
  self.planningBox.hidden = YES;
  self.errorBox.hidden = YES;
}

- (void)statePlanning
{
  self.goButton.hidden = NO;
  self.goButton.enabled = NO;
  self.goButton.enabled = NO;
  self.statusBox.hidden = NO;
  self.resultsBox.hidden = YES;
  self.errorBox.hidden = YES;
  self.planningBox.hidden = NO;
  [self.collectionView reloadData];
  if (IPAD)
    [self iPadNotReady];
}

- (void)stateError
{
  self.goButton.hidden = NO;
  self.goButton.enabled = NO;
  self.statusBox.hidden = NO;
  self.planningBox.hidden = YES;
  self.resultsBox.hidden = YES;
  self.errorBox.hidden = NO;
  if (IPAD)
    [self iPadNotReady];
}

- (void)stateReady
{
  self.goButton.hidden = NO;
  self.goButton.enabled = YES;
  self.statusBox.hidden = NO;
  self.planningBox.hidden = YES;
  self.errorBox.hidden = YES;
  self.resultsBox.hidden = NO;
  if (IPAD)
    [self iPadReady];
}

- (void)iPadReady
{
  [self layoutIfNeeded];
  self.statusBoxHeight.constant = 76.;
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    [self layoutIfNeeded];
  }
  completion:^(BOOL finished)
  {
    [UIView animateWithDuration:kDefaultAnimationDuration animations:^
    {
      self.arriveLabel.alpha = 1.;
    }
    completion:^(BOOL finished)
    {
      self.completeImageView.hidden = NO;
    }];
  }];
}

- (void)iPadNotReady
{
  self.completeImageView.hidden = YES;
  self.arriveLabel.alpha = 0.;
  [self layoutIfNeeded];
  self.statusBoxHeight.constant = 56.;
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    [self layoutIfNeeded];
  }];
}

- (void)reloadData
{
  [self.collectionView reloadData];
}

- (void)selectRouter:(routing::RouterType)routerType
{
  for (auto const & progress : m_progresses)
    progress.second.state = MWMCircularProgressStateNormal;
  m_progresses[routerType].state = MWMCircularProgressStateSelected;
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  [self setupActualHeight];
  if (IPAD)
    [self.delegate routePreviewDidChangeFrame:self.frame];
  [super layoutSubviews];
}

- (void)router:(routing::RouterType)routerType setState:(MWMCircularProgressState)state
{
  m_progresses[routerType].state = state;
}

- (void)router:(routing::RouterType)routerType setProgress:(CGFloat)progress
{
  m_progresses[routerType].progress = progress;
}

#pragma mark - MWMCircularProgressProtocol

- (void)progressButtonPressed:(nonnull MWMCircularProgress *)progress
{
  [Statistics logEvent:kStatEventName(kStatNavigationDashboard, kStatButton)
        withParameters:@{kStatValue : kStatProgress}];
  MWMCircularProgressState const s = progress.state;
  if (s == MWMCircularProgressStateSelected || s == MWMCircularProgressStateCompleted)
    return;

  for (auto const & prg : m_progresses)
  {
    if (prg.second != progress)
      continue;
    routing::RouterType const router = prg.first;
    [self.dashboardManager setActiveRouter:router];
    [self selectRouter:router];
    switch (router)
    {
      case routing::RouterType::Vehicle:
        [Statistics
         logEvent:kStatPointToPoint
         withParameters:@{kStatAction : kStatChangeRoutingMode, kStatValue : kStatVehicle}];
        break;
      case routing::RouterType::Pedestrian:
        [Statistics
         logEvent:kStatPointToPoint
         withParameters:@{kStatAction : kStatChangeRoutingMode, kStatValue : kStatPedestrian}];
        break;
      case routing::RouterType::Bicycle:
        [Statistics
         logEvent:kStatPointToPoint
         withParameters:@{kStatAction : kStatChangeRoutingMode, kStatValue : kStatBicycle}];
        break;
    }
  }
}

#pragma mark - Properties

- (CGRect)defaultFrame
{
  if (!IPAD)
    return super.defaultFrame;
  CGFloat const width = 320.;
  CGFloat const origin = self.isVisible ? 0. : -width;
  return {{origin, self.topBound}, {width, self.superview.height - kAdditionalHeight}};
}

- (CGFloat)visibleHeight
{
  return self.planningRouteViewHeight.constant + self.planningContainerHeight.constant + kAdditionalHeight;
}

- (IBAction)extendTap
{
  BOOL const isExtended = !self.extendButton.selected;
  [Statistics logEvent:kStatEventName(kStatPointToPoint, kStatExpand)
                   withParameters:@{kStatValue : (isExtended ? kStatYes : kStatNo)}];
  self.extendButton.selected = isExtended;
  [self layoutIfNeeded];
  [self setupActualHeight];
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    self.arrowImageView.transform = isExtended ? CGAffineTransformMakeRotation(M_PI) : CGAffineTransformIdentity;
    [self layoutIfNeeded];
  }];
}

- (void)setupActualHeight
{
  if (!self.superview)
    return;
  if (IPAD)
  {
    CGFloat const selfHeight = self.superview.height - kAdditionalHeight;
    self.defaultHeight = selfHeight;
    self.height = selfHeight;
    return;
  }
  BOOL const isPortrait = self.superview.height > self.superview.width;
  CGFloat const height = isPortrait ? 140. : 96.;
  CGFloat const planningRouteViewHeight = self.extendButton.selected ? height : 44.;
  self.planningRouteViewHeight.constant = planningRouteViewHeight;
  CGFloat const selfHeight = planningRouteViewHeight + self.planningContainerHeight.constant;
  self.defaultHeight = selfHeight;
  self.height = selfHeight;
  [self.dashboardManager.delegate routePreviewDidChangeFrame:{self.origin, {self.width, selfHeight + kAdditionalHeight}}];
}

- (void)setDataSource:(id<MWMRoutePreviewDataSource>)dataSource
{
  _dataSource = dataSource;
  [self reloadData];
}

- (void)snapshotCell:(MWMRoutePointCell *)cell
{
  UIGraphicsBeginImageContextWithOptions(cell.bounds.size, NO, 0.);
  [cell drawViewHierarchyInRect:cell.bounds afterScreenUpdates:YES];
  UIImage * image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  self.movingCellImage = [[UIImageView alloc] initWithImage:image];
  self.movingCellImage.alpha = 0.8;
  [self.collectionView addSubview:self.movingCellImage];
  CALayer * l = self.movingCellImage.layer;
  l.masksToBounds = NO;
  l.shadowColor = UIColor.blackColor.CGColor;
  l.shadowRadius = 4.;
  l.shadowOpacity = 0.4;
  l.shadowOffset = {0., 0.};
  l.shouldRasterize = YES;
  l.rasterizationScale = [[UIScreen mainScreen] scale];
}

- (NSDictionary *)etaAttributes
{
  return @{NSForegroundColorAttributeName : UIColor.blackPrimaryText,
           NSFontAttributeName : UIFont.medium17};
}

#pragma mark - MWMRoutePointCellDelegate

- (void)startEditingCell:(MWMRoutePointCell *)cell
{
  NSUInteger const index = [self.collectionView indexPathForCell:cell].row;
  [self.dashboardManager.delegate didStartEditingRoutePoint:index == 0];
}

- (void)swapPoints
{
  [self.dashboardManager.delegate swapPointsAndRebuildRouteIfPossible];
}

#pragma mark - PanGestureRecognizer

- (void)didPan:(UIPanGestureRecognizer *)pan cell:(MWMRoutePointCell *)cell
{
  CGPoint const locationPoint = [pan locationInView:self.collectionView];
  if (pan.state == UIGestureRecognizerStateBegan)
  {
    self.layout.isNeedToInitialLayout = NO;
    self.isNeedToMove = NO;
    self.indexPathOfMovingCell = [self.collectionView indexPathForCell:cell];
    [self snapshotCell:cell];
    [UIView animateWithDuration:kDefaultAnimationDuration animations:^
    {
      cell.contentView.alpha = 0.;
      CGFloat const scaleY = 1.05;
      self.movingCellImage.transform = CGAffineTransformMakeScale(1., scaleY);
    }];
  }

  if (pan.state == UIGestureRecognizerStateChanged)
  {
    self.movingCellImage.center = {locationPoint.x - cell.width / 2 + 30, locationPoint.y};
    NSIndexPath * finalIndexPath = [self.collectionView indexPathForItemAtPoint:locationPoint];
    if (finalIndexPath && ![finalIndexPath isEqual:self.indexPathOfMovingCell])
    {
      if (self.isNeedToMove)
        return;
      self.isNeedToMove = YES;
      [self.collectionView performBatchUpdates:^
      {
        [self.collectionView moveItemAtIndexPath:finalIndexPath toIndexPath:self.indexPathOfMovingCell];
        self.indexPathOfMovingCell = finalIndexPath;
      } completion:nil];
    }
    else
    {
      self.isNeedToMove = NO;
    }
  }

  if (pan.state == UIGestureRecognizerStateEnded)
  {
    self.layout.isNeedToInitialLayout = YES;
    NSIndexPath * finalIndexPath = [self.collectionView indexPathForItemAtPoint:locationPoint];
    self.isNeedToMove = finalIndexPath && ![finalIndexPath isEqual:self.indexPathOfMovingCell];
    if (self.isNeedToMove)
    {
      cell.contentView.alpha = 1.;
      [self.collectionView performBatchUpdates:^
      {
        [self.collectionView moveItemAtIndexPath:self.indexPathOfMovingCell toIndexPath:finalIndexPath];
      }
      completion:^(BOOL finished)
      {
        [self.movingCellImage removeFromSuperview];
        self.movingCellImage.transform = CGAffineTransformIdentity;
      }];
    }
    else
    {
      cell.contentView.alpha = 1.;
      [self.movingCellImage removeFromSuperview];
      [self swapPoints];
      [self reloadData];
    }
  }
}

@end

#pragma mark - UICollectionView

@interface MWMRoutePreview (UICollectionView) <UICollectionViewDataSource, UICollectionViewDelegate>

@end

@implementation MWMRoutePreview (UICollectionView)

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
  return 2;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
  MWMRoutePointCell * cell = [collectionView dequeueReusableCellWithReuseIdentifier:[MWMRoutePointCell className] forIndexPath:indexPath];
  cell.number.text = @(indexPath.row + 1).stringValue;
  if (indexPath.row == 0)
  {
    cell.title.text = self.dataSource.source;
    cell.title.placeholder = L(@"p2p_from");
  }
  else
  {
    cell.title.text = self.dataSource.destination;
    cell.title.placeholder = L(@"p2p_to");
  }
  cell.delegate = self;
  return cell;
}

@end
