#pragma once

class HandSystem
{
public:
	bool AddCard(GameObject* obj)
	{
		if (hand.size() >= MAX_HAND_SIZE)return false;
		hand.push_back(obj);
		return true;
	}

	GameObject* GetCardAt(int index)
	{
		return hand[index];
	}

private:
	const int MAX_HAND_SIZE = 10;
	std::vector<GameObject*> hand;
};