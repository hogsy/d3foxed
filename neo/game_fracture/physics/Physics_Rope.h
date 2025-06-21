/*=========================================================================
    d3foxed, a modified fork of the fhDOOM engine.
    Copyright (C) 2024 Mark E. Sowden <hogsy@oldtimes-software.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
=========================================================================*/

#pragma once

#include <vector>

namespace ms
{
	class Physics_Rope : public idPhysics_Base
	{
	public:
		CLASS_PROTOTYPE( Physics_Rope );

		explicit Physics_Rope( float length = 8.0f, float mass = 0.05f );
		Physics_Rope( idVec3 start, idVec3 end, float length = 8.0f, float mass = 0.05f );
		explicit Physics_Rope( idVec3 start, float length = 8.0f, float mass = 0.05f );
		~Physics_Rope() override;

		void Save( idSaveGame *saveGame ) const;
		void Restore( idRestoreGame *restoreGame );

		void WriteToSnapshot( idBitMsgDelta &msg ) const override;
		void ReadFromSnapshot( const idBitMsgDelta &msg ) override;

	private:
		struct Particle
		{
			idVec3 position;   // our actual position
			idVec3 oldPosition;// the last actual position

			idVec3 velocity;

			bool fixed{};// if fixed, won't be simulated

			bool CheckForCollisions( const idVec3 &newPosition, trace_t &collision ) const;
		};

	public:
		bool Evaluate( int timeStepMSec, int endTimeMSec ) override;

		inline void SetSegments( unsigned int num )
		{
			if ( num == particles.size() )
			{
				return;
			}
			else if ( num < 2 )
			{
				common->Warning( "Invalid number of segments for rope (%u); must be greater than 2!\n", num );
				return;
			}

			Particle start = particles.front();
			Particle end   = particles.back();

			Detach( true );
			Detach( false );

			particles.resize( num );

			particles.front() = start;
			particles.back()  = end;
		}

		void Attach( const idVec3 &position, bool start )
		{
			Particle &slot = start ? particles[ 0 ] : particles[ particles.size() - 1 ];
			slot.fixed     = true;
			slot.position  = position;
		}

		void Detach( bool start )
		{
			if ( particles.empty() )
			{
				return;
			}

			particles[ start ? 0 : particles.size() - 1 ].fixed = false;
		}

	private:
		void DebugDraw();

		inline float GetAverageSegmentLength() const
		{
			return length / ( float ) ( particles.size() - 1 );
		}

		inline float GetCurrentRopeLength() const
		{
			float l = 0.0f;
			for ( unsigned int i = 0; i < particles.size() - 1; ++i )
			{
				l += ( particles[ i + 1 ].position - particles[ i ].position ).Length();
			}

			return l;
		}

		static constexpr unsigned int DEFAULT_NUM_PARTICLES = 8;
		std::vector< Particle >       particles;

		// various properties shared between all the particles

		float width{ 0.5f };
		float length{ 8.0f };
		float mass{ 0.05f };
	};
}// namespace ms
