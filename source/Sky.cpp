#include <cassert>
#include "Application.h"
#include "Game.h"
#include "Math/Rand.h"
#include "Sky.h"
#include "Sprite.h"

using namespace OT;


Sky::Sky(Game * game) : GameObject(game) {
	from     = 0;
	to       = 0;
	progress = 0;
	rainyDay = false;
	rainAnimation = 0;
	soundCountdown = 0;
	thunderOverlay = 0;

	rainSound.setBuffer(app->sounds["simtower/rain"]);
	rainSound.setLoop(true);
	thunderSound.setBuffer(app->sounds["simtower/thunder"]);
	birdsSound.setBuffer(app->sounds["simtower/birds/day"]);
	cricketsSound.setBuffer(app->sounds["simtower/crickets"]);
}

void Sky::advance(double dt)
{
	//Decide whether we're about to have a rainy day.
	if (game->time.checkHour(5)) {
		rainyDay = ((rand() % 3) == 0); //Statistically, every 3rd day will be rainy.
		rainAnimation = 0;
		//game->timeWindow.showMessage(rainyDay ? "I've heard we're in for some bad weather..." : "Weather's going to be good today!");
	}

	/* Decide what sky color to use based on the current time of day. The drawing code will
	 * interpolate between the sky texture offsets 'from' and 'to'. So to get a transition
	 * from day to dusk, from = 0, to = 1 and progress is anywhere between 0..1. */
	double time = game->time.hour;
	double dta = game->time.dta / Time::kBaseSpeed;

	//Night
	if (time < 5 || time >= 19) {
		from = to = 2; progress = 0;
	}

	//Dawn
	else if (time >= 5 && time < 6) {
		from = 2; to = 1; progress = (time-5);
	}
	else if (time >= 6 && time < 7) {
		from = 1; to = 0; progress = (time-6);
	}

	//Dusk
	else if (time >= 17 && time < 18) {
		from = 0; to = 1; progress = (time-17);
	}
	else if (time >= 18 && time < 19) {
		from = 1; to = 2; progress = (time-18);
	}

	//Day
	else if (!rainyDay) {
		from = to = 0; progress = 0;
	}

	//Rain
	else if (time >= 7 && time < 8) {
		from = 0; to = 3; progress = (time-7);
	}
	else if (time >= 16 && time < 17) {
		from = 3; to = 0; progress = (time-16);
	}
	else {
		rainAnimation += dta;
		from = to = 4+floor(fmod(rainAnimation, 1) * 2); progress = 0;
	}

	//Rain sounds.
	if (rainyDay) {
		if (game->time.checkHour(8))  rainSound.Play(game);
		if (game->time.checkHour(16)) rainSound.Stop();
	}

	if (thunderOverlay > 0) {
		thunderOverlay *= exp(-dta * 7);
		if (thunderOverlay < 1e-3) thunderOverlay = 0;
	}

	//Sounds.
	soundCountdown -= dta;
	if (soundCountdown < 0) {
		double duration = 0;
		if (rainyDay && time >= 8 && time < 16) {
			thunderSound.Play(game);
			thunderOverlay = 1;
			duration = thunderSound.getDurationDouble();
		} else if (time >= 8 && time < 17) {
			birdsSound.Play(game);
			duration = birdsSound.getDurationDouble();
		} else if (time >= 20 || time < 1.5) {
			cricketsSound.Play(game);
			duration = cricketsSound.getDurationDouble();
		}
		soundCountdown += Math::randd(duration + 0.5, duration + 10);
	}
}

void Sky::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
    render(target);
}

void Sky::render(sf::RenderTarget & target) const
{
	const sf::View &view = target.getView();
	sf::Vector2f center = view.getCenter();
	sf::Vector2f size = view.getSize();
	sf::Vector2f dmax = center + size/2.f;
	sf::Vector2f dmin = center - size/2.f;

	//Draw the sky color.
	int sky_lower = std::max<int>(floor(-dmax.y / 360), -1);
	int sky_upper = std::min<int>(ceil (-dmin.y / 360), 11);

	Sprite sky;
	sky.SetImage(app->bitmaps["simtower/sky"]);
	for (int y = sky_lower; y <= sky_upper; y++) {
		for (int i = 0; i < 2; i++) {
			if ((i == 0 && progress == 1) || (i == 1 && progress == 0)) continue;

			int index = (std::min<int>(y + 1, 9) * 6 + (i == 0 ? from : to));
			sky.setTextureRect(sf::IntRect(index * 32, 0,  32, 360));
			sky.setOrigin(0, 360);
			sky.setColor(sf::Color(255, 255, 255, 255*(i == 0 ? 1.0 : progress)));

			for (int x = floor(dmin.x / 32); x < ceil(dmax.x / 32); x++) {
				sky.setPosition(x * 32, -y * 360);
				target.draw(sky);
				game->drawnSprites++;
			}
		}
	}

	//Draw the clouds.
	Sprite cloud;
	int2 cloudGrid(250, 100);
	int2 cmin(floor(dmin.x / cloudGrid.x)-1, floor(-dmax.y / cloudGrid.y)-1);
	int2 cmax(ceil(dmax.x / cloudGrid.x)+1, ceil(-dmin.y / cloudGrid.y)+1);
	if (cmin.y < 2) cmin.y = 2;
	for (int x = cmin.x; x <= cmax.x; x++) {
		for (int y = cmin.y; y <= cmax.y; y++) {
			int2 position(x*cloudGrid.x, -y*cloudGrid.y);

			//Decide whether this location should have a cloud, based on the cloud noise
			//function.
			if (cloudNoise(position*45) < 0.3) continue;

			//Decide what cloud variant to use. Multiplying the position with some arbitrary
			//large number introduces a higher noise frequency so the clouds look more random.
			//(...+1)*2 converts the noise range [-1,1] to [0,3].
			int variant = (cloudNoise(position * 4672) + 1) * 2;
			assert(variant >= 0 && variant <= 3);

			//Introduce some jitter so the clouds don't look like they're on a grid.
			double2 offset(cloudNoise(position * 941), cloudNoise(position * 786));
			position += offset * 50;

			char c[128];
			snprintf(c, 128, "simtower/deco/cloud/%i", abs(variant)%4);
			cloud.SetImage(app->bitmaps[c]);
			sf::Vector2u size = cloud.getTexture()->getSize();
			int w = size.x;
			int h = size.y / 4;

			for (int i = 0; i < 2; i++) {
				if ((i == 0 && progress == 1) || (i == 1 && progress == 0)) continue;
				int state = std::min<int>(3, i == 0 ? from : to);

				cloud.setTextureRect(sf::IntRect(0, state*h, w, h));
				// cloud.setScale(w, h);
				cloud.setOrigin(w/2, h/2);
				cloud.setColor(sf::Color(255, 255, 255, 255*(i == 0 ? 1.0 : progress)));
				cloud.setPosition(position.x, position.y);
				target.draw(cloud);
				game->drawnSprites++;
			}
		}
	}

	//Draw the thunder overlay.
	if (thunderOverlay > 0) {
		float miny = std::min<float>(dmin.y, 0.f);
		float maxy = std::min<float>(dmax.y, 0.f);

		sf::RectangleShape s(sf::Vector2f(size.x, maxy-miny));
		s.setFillColor(sf::Color(255, 255, 255, 255*thunderOverlay));
		s.setPosition(sf::Vector2f(dmin.x, miny));
		target.draw(s);
		game->drawnSprites++;
	}

	//Draw the skyline, if in view.
	if (-dmax.y <= 96 && -dmin.y >= 0) {
		Sprite city;
		city.SetImage(app->bitmaps["simtower/deco/skyline"]);
		city.setOrigin(0, 55);
		for (int x = floor(dmin.x / 96); x < ceil(dmax.x / 96); x++) {
			city.setPosition(x * 96, 0);
			target.draw(city);
			game->drawnSprites++;
		}
	}
}

double Sky::cloudNoise(double2 p)
{
	int n=(int)p.x+(int)p.y*57;
	n=(n<<13)^n;
	int nn=(n*(n*n*60493+19990303)+1376312589)&0x7fffffff;
	return 1.0-((double)nn/1073741824.0);
}
