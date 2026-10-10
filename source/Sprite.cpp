#include "Sprite.h"
#include "Application.h"
#include "OpenGL.h"

using namespace OT;

// void Sprite::draw(sf::RenderTarget& target, sf::RenderStates states) const
// {
// 	glTranslatef(-0.375f, -0.375f, 0.f);
// 	target.draw((const sf::Sprite&)*this, states);
// }

void Sprite::SetImage(const sf::Image& image)
{
	texture.loadFromImage(image);
	setTexture(texture, true);
}

void Sprite::SetImage(const sf::Texture& texture)
{
	setTexture(texture, true);
}

// Device pixels per world unit for the target's current view.
static float pixelsPerUnit(const sf::RenderTarget & target)
{
	const sf::View & view = target.getView();
	if (view.getSize().x == 0.f)
		return 1.f;
	return target.getSize().x * view.getViewport().size.x / view.getSize().x;
}

void Sprite::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
	unsigned k = 0;
	const sf::Texture * hi = nullptr;
	if (App && pixelsPerUnit(target) > 1.01f)
		hi = App->bitmaps.hiresFor(&getTexture(), k);
	if (hi && k > 1) {
		const sf::IntRect r = getTextureRect();
		const int s = static_cast<int>(k);
		sf::Sprite twin(*hi, sf::IntRect({r.position.x * s, r.position.y * s}, {r.size.x * s, r.size.y * s}));
		twin.setColor(getColor());
		states.transform *= getTransform();
		states.transform.scale({1.f / k, 1.f / k});
		target.draw(twin, states);
		return;
	}
	// sf::Sprite::draw is private: draw a sliced copy, which renders exactly
	// like the base class.
	const sf::Sprite plain(static_cast<const sf::Sprite &>(*this));
	target.draw(plain, states);
}
