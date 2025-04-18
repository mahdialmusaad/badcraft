#pragma once
#ifndef _SOURCE_RENDERING_TEXTRENDERER_HDR_
#define _SOURCE_RENDERING_TEXTRENDERER_HDR_

#include <Utility/Definitions.hpp>

class TextRenderer
{
public:
	struct ScreenText
	{
		enum class TextType : uint8_t {
			Default = 0, 
			Temporary = 1, 
			TemporaryShow = 2,
			Hidden = 3
		};

		typedef glm::vec<3, uint8_t> RGBVector;
		
		ScreenText(glm::vec2 pos, std::string text, TextType type, uint8_t fontSize) noexcept;

		void ResetTextTime() noexcept;

		void _ChangeInternalText(std::string newText) noexcept;
		void _ChangeInternalPosition(glm::vec2 newPos) noexcept;
		void _ChangeInternalColour(RGBVector newColour) noexcept;
		void _ChangeInternalFontSize(uint8_t newFontSize) noexcept;

		glm::vec2 GetPosition() const noexcept;
		int GetDisplayLength() const noexcept;
		std::string& GetText() noexcept;
		RGBVector GetColour() const noexcept;
		uint8_t GetFontSize() const noexcept;
		float GetTime() const noexcept;
	private:
		std::string m_text;
		glm::vec2 m_pos = { 0.0f, 0.0f };
		float m_textTime = 0.0f;

		uint16_t m_displayLength = 0u;
		uint8_t m_fontSize = 16u;

		RGBVector m_RGBColour = { 255, 255u, 255u };
	public:
		TextType type = TextType::Default;
		uint8_t beQuietPlease = false;
		GLuint vbo;
	};

	typedef ScreenText::TextType T_Type;

	static constexpr float TEMP_TIME = 5.0f;
	float letterSpacing = 0.002f;
	float lineSpacing = 0.04f;
	uint8_t textVAO;

	TextRenderer() noexcept;
	void RenderText() const noexcept;

	ScreenText* CreateText(glm::vec2 pos, std::string text, T_Type textType = T_Type::Default, uint8_t fontSize = 16) noexcept;
	void RecalculateAllText() noexcept;

	ScreenText* GetTextFromID(uint16_t id) noexcept;
	uint16_t GetIDFromText(ScreenText* screenText) const noexcept;

	void ChangeText(ScreenText* screenText, std::string newText) noexcept;
	void ChangePosition(ScreenText* screenText, glm::vec2 newPos) noexcept;
	void ChangeFontSize(ScreenText* screenText, uint8_t newFontSize) noexcept;
	void ChangeColour(ScreenText* screenText, ScreenText::RGBVector newColour) noexcept;

	void CheckTextStatus() noexcept;
	void RemoveText(uint16_t id) noexcept;
private:
	static constexpr uint8_t m_charSizes[] = { 
		1, 3, 5, 5, 5, 5, 1, 2, 2, 3, 
		3, 2, 3, 1, 3, 4, 3, 3, 3, 4, 
		3, 3, 4, 4, 3, 1, 2, 3, 3, 3, 
		3, 7, 4, 4, 4, 4, 4, 4, 4, 4, 
		3, 4, 4, 4, 5, 4, 4, 4, 5, 4, 
		4, 3, 4, 5, 5, 5, 5, 4, 2, 3, 
		2, 3, 4, 2, 5, 4, 3, 4, 4, 4, 
		4, 4, 1, 2, 3, 3, 6, 4, 4, 4, 
		4, 3, 4, 3, 4, 3, 5, 3, 4, 4, 
		3, 1, 3, 4 
	};
	std::unordered_map<uint16_t, ScreenText*> m_screenTexts;

	void UpdateText(ScreenText* screenText) const noexcept;
	uint16_t GetNewID() noexcept;
};

#endif // _TEXTRENDERER_HEADER_
