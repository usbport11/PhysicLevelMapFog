#include "../../../stdafx.h"
#include "Doodad.h"

MDoodad::MDoodad():MPhysicQuad() {
}

bool MDoodad::Set(glm::vec2 vp, glm::vec2 vs, glm::vec2 tp, glm::vec2 ts) {
	return MPhysicQuad::Set(vp, vs, tp, ts, b2_dynamicBody, OT_DOODAD, OT_BULLET | OT_HERO | OT_ENEMY | OT_WALL | OT_DOODAD, false, 4.5);
}

void MDoodad::OnBeginCollide(b2Fixture* MainFixture, b2Fixture* ObjectFixture) {
	if(ObjectFixture->GetFilterData().categoryBits == OT_BULLET) NeedRemove = true;
}

void MDoodad::OnEndCollide(b2Fixture* MainFixture, b2Fixture* ObjectFixture) {}
void MDoodad::OnBeginWallCollide() {}
void MDoodad::OnEndWallCollide() {}
