#ifndef BOX2DUTILS_H
#define BOX2DUTILS_H

#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"

namespace simu {

struct Box2DUtils {
  static b2Body* clone (b2Body *oldBody, b2World *newWorld) {
    b2BodyDef def;

#define SET(l,U,s) def.l##s = oldBody->Get##U##s()
    SET(t,T,ype);
    SET(p,P,osition);
    SET(a,A,ngle);
    SET(l,L,inearVelocity);
    SET(a,A,ngularVelocity);
    SET(l,L,inearDamping);
    SET(a,A,ngularDamping);
    SET(g,G,ravityScale);
#undef SET
#define SET(l,U,s) def.l##s = oldBody->Is##U##s()
    SET(e,E,nabled);
    SET(a,A,wake);
    SET(f,F,ixedRotation);
    SET(b,B,ullet);
#undef SET

    def.allowSleep = oldBody->IsSleepingAllowed();

    return newWorld->CreateBody(&def);
  }

  static b2Fixture* clone (const b2Fixture *oldFixture, b2Body *newBody) {
    b2FixtureDef def;
    def.shape = oldFixture->GetShape();

    def.friction = oldFixture->GetFriction();
    def.restitution = oldFixture->GetRestitution();
    def.density = oldFixture->GetDensity();
    def.isSensor = oldFixture->IsSensor();
    def.filter = oldFixture->GetFilterData();

    return newBody->CreateFixture(&def);
  }
};

} // end of namespace simu

void assertEqual(const b2Body &lhs, const b2Body &rhs, bool deepcopy);
void assertEqual(const b2Fixture &lhs, const b2Fixture &rhs, bool deepcopy);

#endif // BOX2DUTILS_H
