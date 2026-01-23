#pragma once
namespace RE
{
	namespace Offset
	{
		typedef void(_fastcall* _destroyProjectile)(RE::Projectile* a_projectile);
		inline static REL::Relocation<_destroyProjectile> destroyProjectile{ RELOCATION_ID(42930, 44110) };

	}
}