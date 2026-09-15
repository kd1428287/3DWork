#include "GameObject.h"
#include "ObjectManager.h"

void GameObject::RequestAwake(ComponentBase* component)
{
	context_->objectManager->RequestAwake(component);
}
