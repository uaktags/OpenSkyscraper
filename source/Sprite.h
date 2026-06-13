#pragma once
#include <SFML/Graphics.hpp>

namespace OT
{
	inline sf::Texture &getEmptyTexture()
	{
		static sf::Texture tex;
		return tex;
	}
	class Sprite : public sf::Sprite
	{
	public:
		// Default constructor
		Sprite() : sf::Sprite(getEmptyTexture()) {}

		// Copy and move support
		Sprite(const Sprite &) = default;
		Sprite(Sprite &&) noexcept = default;
		Sprite &operator=(const Sprite &) = default;
		Sprite &operator=(Sprite &&) noexcept = default;

		// Image/Texture assignment
		void SetImage(const sf::Image &image);
		void SetImage(const sf::Texture &texture);

		// Get size from texture rectangle
		sf::Vector2u getSize() const
		{
			auto bounds = getLocalBounds();
			return sf::Vector2u(static_cast<unsigned>(bounds.size.x), static_cast<unsigned>(bounds.size.y));
		}

	protected:
		sf::Texture texture;
	};
}
