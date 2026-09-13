#pragma once

class IRawDirectionSource {
public:
	virtual ~IRawDirectionSource() = default;
	virtual Math::Vector3 GetRawDirection() = 0;
};