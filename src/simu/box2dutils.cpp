#include "box2dutils.h"

#include "kgd/utils/assertequal.hpp"
#include "config.h"

void assertEqual(const b2Body &lhs, const b2Body &rhs, bool deepcopy) {
  using utils::assertEqual;
  if (deepcopy) utils::assertDeepcopy(lhs, rhs);

#define ASRT(X) assertEqual(lhs.X(), rhs.X(), false)
  ASRT(GetType);
  ASRT(GetPosition);
  ASRT(GetAngle);
  ASRT(GetLinearVelocity);
  ASRT(GetAngularVelocity);
  ASRT(GetLinearDamping);
  ASRT(GetAngularDamping);
  ASRT(GetGravityScale);
  ASRT(IsEnabled);
  ASRT(IsAwake);
  ASRT(IsFixedRotation);
  ASRT(IsBullet);
  ASRT(IsSleepingAllowed);
#undef ASRT

  auto flhs = lhs.GetFixtureList(), frhs = rhs.GetFixtureList();
  while (flhs && frhs) {
    assertEqual(flhs, frhs, deepcopy);
    flhs = flhs->GetNext();
    frhs = frhs->GetNext();
  }

  assertEqual(flhs, frhs, false);
}

void assertEqual (const b2Filter &lhs, const b2Filter &rhs, bool deepcopy) {
  utils::assertEqual(lhs.categoryBits, rhs.categoryBits, deepcopy);
  utils::assertEqual(lhs.maskBits, rhs.maskBits, deepcopy);
  utils::assertEqual(lhs.groupIndex, rhs.groupIndex, deepcopy);
}

void assertEqual(const b2Fixture &lhs, const b2Fixture &rhs, bool deepcopy) {
  using utils::assertEqual;
  if (deepcopy) utils::assertDeepcopy(lhs, rhs);

#define ASRT(X) assertEqual(lhs.X(), rhs.X(), false)
  ASRT(GetShape()->GetType);
  ASRT(GetFriction);
  ASRT(GetRestitution);
  ASRT(GetDensity);
  ASRT(IsSensor);
  ASRT(GetFilterData);
#undef ASRT
}
