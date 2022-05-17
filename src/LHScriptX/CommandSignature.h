/*****************************************************************************
 * Copyright (c) 2018-2022 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace openblack
{
class Game;
}

namespace openblack::lhscriptx
{
enum class ParameterType
{
	None = 0,
	String, // L
	Float,  // F
	Number, // N
	Vector  // A
};

class ScriptCommandParameter
{
public:
	ScriptCommandParameter(ParameterType type = ParameterType::None)
	    : _type(type)
	{
	}
	ScriptCommandParameter(const std::string& value)
	    : _type(ParameterType::String)
	{
		_string = value;
	}
	ScriptCommandParameter(float value)
	    : _type(ParameterType::Float)
	{
		SetFloat(value);
	}
	ScriptCommandParameter(int32_t value)
	    : _type(ParameterType::Number)
	{
		SetNumber(value);
	}
	ScriptCommandParameter(float x, float y, float z)
	    : _type(ParameterType::Vector)
	{
		SetVector(x, y, z);
	}

	[[nodiscard]] ParameterType GetType() const { return _type; };

	[[nodiscard]] const std::string& GetString() const { return _string; }
	[[nodiscard]] float GetFloat() const { return _value._float; }
	[[nodiscard]] int32_t GetNumber() const { return _value._number; }
	void GetVector(float& x, float& y, float& z) const
	{
		x = _value._vector[0];
		y = _value._vector[1];
		z = _value._vector[2];
	}

	void SetString(const std::string& value) { _string = value; }
	void SetFloat(float value) { _value._float = value; };
	void SetNumber(int32_t value) { _value._number = value; }
	void SetVector(float x, float y, float z)
	{
		_value._vector[0] = x;
		_value._vector[1] = y;
		_value._vector[2] = z;
	}

private:
	ParameterType _type;

	union
	{
		float _float;
		int32_t _number;
		// NOLINTNEXTLINE(modernize-avoid-c-arrays): array won't work in union
		float _vector[3];
	} _value;
	std::string _string;
};

typedef std::vector<ScriptCommandParameter> ScriptCommandParameters;

class ScriptCommandContext
{
public:
	ScriptCommandContext(Game* game, const ScriptCommandParameters* parameters)
	    : _game(game)
	    , _parameters(parameters)
	{
	}

	[[nodiscard]] Game& GetGame() const { return *_game; }

	[[nodiscard]] const ScriptCommandParameters& GetParameters() const { return *_parameters; }

	const ScriptCommandParameter& operator[](uint32_t arg) const { return _parameters->at(arg); }

private:
	Game* _game;
	const ScriptCommandParameters* _parameters;
};

typedef std::function<void(const ScriptCommandContext&)> ScriptCommand;

struct ScriptCommandSignature
{
	const std::array<char, 0x80> name;
	const ScriptCommand command;
	const std::array<ParameterType, 9> parameters;
};
} // namespace openblack::lhscriptx
