#pragma once
#include "../Transform/TransformComponent.h" 

class IRawDirectionSource {
public:
	virtual ~IRawDirectionSource() = default;
	virtual Math::Vector3 GetRawDirection() = 0;
};