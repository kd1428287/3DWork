#pragma once

#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Physics/ColliderParams.h"

COMPONENT_PARAMS_DEFINE_TYPE(ColliderShapeParams,
	name, shape, offset, radius, capsuleEnd, halfExtents,
	categoryMask, useDefaultCollideMask, collideMask, enabled, isTrigger, isStatic, wantsStayEvent)

COMPONENT_PARAMS_DEFINE_TYPE(ColliderParams, shapes, bodyShapeName, wireFrame, ignoreParent)
