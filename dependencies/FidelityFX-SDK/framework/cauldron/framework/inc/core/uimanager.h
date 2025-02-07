// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include "misc/helpers.h"
#include "misc/assert.h"

#include <functional>
#include <map>
#include <vector>


namespace cauldron
{
    /**
     * @enum UIElementType
     *
     * UI element types to pick from, add more as needed.
     *
     * @ingroup CauldronCore
     */
    enum class UIElementType
    {
        Text = 0,           ///< Text UI element
        Button,             ///< Button UI element
        Checkbox,           ///< Checkbox UI element
        RadioButton,        ///< Radio button UI element

        Combo,              ///< Combo box UI element
        Slider,             ///< Slider UI element

        Separator,          ///< Separator UI element

        Count               ///< UI element count
    };

    /**
     * @enum UIElement
     *
     * Describes as a base class of an individual UI element (text, butotn, checkbox, etc.).
     *
     * @ingroup CauldronCore
     */
    class UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI element.
         */
        UIElement(uint64_t id, UIElementType type, const char* description,
            const bool& enableControl, bool shown, bool sameLineAsPreviousElement)
            : m_ID(id)
            , m_Type(type)
            , m_Description(description)
            , m_EnableControl(enableControl)
            , m_Shown(shown)
            , m_SameLineAsPreviousElement(sameLineAsPreviousElement)
        {}

        /**
         * @brief   Destructor. Destroys the UI element.
         */
        virtual ~UIElement() = default;

        /**
         * @brief   Get the element type.
        */
        inline UIElementType GetType() const noexcept
        {
            return m_Type;
        }

        /**
         * @brief   Get the element description.
        */
        inline const char* GetDesc() const noexcept
        {
            return m_Description.c_str();
        }

        /**
         * @brief   Set the element type.
        */
        inline void SetDesc(const char* description)
        {
            m_Description = description;
        }

        /**
         * @brief   Check if the element is enabled.
        */
        inline bool Enabled() const noexcept
        {
            return m_EnableControl;
        }

        /**
         * @brief   Check if the element is same line.
        */
        inline bool SameLine() const noexcept
        {
            return m_SameLineAsPreviousElement;
        }

        /**
         * @brief   Get the element ID.
        */
        inline uint64_t ID() const noexcept
        {
            return m_ID;
        }

        /**
         * @brief   Check the element is shown.
        */
        inline bool IsShown() const noexcept
        {
            return m_Shown;
        }

        /**
         * @brief   Show or hide the element.
        */
        inline void Show(bool show) noexcept
        {
            m_Shown = show;
        }

        /**
         * @brief   Release UIElement.
        */
        inline void Release()
        {
            delete this;
        }

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() = 0;

    protected:
        static const bool AlwaysEnable = true; // Default read only enable flag for always enabled element

    private:
        uint64_t m_ID = 0;
        const UIElementType m_Type = UIElementType::Count;     // Type of ui element
        std::string         m_Description = "";                // Description of ui element
        const bool&         m_EnableControl;                   // To allow conditional enabling of UI element
        bool                m_Shown                     = true;
        bool                m_SameLineAsPreviousElement = false;
    };

    /**
     * @enum UIText
     *
     * Describes of UI Text element.
     *
     * @ingroup CauldronCore
    */
    class UIText : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI text.
         */
        UIText(uint64_t id, const char* text, const bool& enable, bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::Text, text, enable, shown, sameLine)
        {}
        
        /**
         * @brief   Constructor. Creates the UI text without enabler.
         */
        UIText(uint64_t id, const char* text, bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::Text, text, AlwaysEnable, shown, sameLine)
        {}

        /**
         * @brief   Destructor. Destroys the UI text.
         */
        virtual ~UIText() = default;

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;
    };

    /**
     * @enum UIButton
     *
     * Describes of UI Button element.
     *
     * @ingroup CauldronCore
    */
    class UIButton : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI button.
         */
        UIButton(uint64_t id, const char* text, const bool& enable, std::function<void(void)> callback,
            bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::Button, text, enable, shown, sameLine)
            , m_Callback(std::move(callback))
        {}

        /**
         * @brief   Constructor. Creates the UI button.
         */
        UIButton(uint64_t id, const char* text, std::function<void(void)> callback, bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::Button, text, AlwaysEnable, shown, sameLine)
            , m_Callback(std::move(callback))
        {
        }

        /**
         * @brief   Destructor. Destroys the UI button.
         */
        virtual ~UIButton() = default;

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;

    private:
        std::function<void(void)> m_Callback;
    };

    /**
     * @enum UICheckBox
     *
     * Describes of UI Chekbox element.
     *
     * @ingroup CauldronCore
    */
    class UICheckBox : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI checkbox.
         */
        UICheckBox(uint64_t id, const char* text, bool& data, const bool& enable,
            std::function<void(bool, bool)> callback = nullptr, bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::Checkbox, text, enable, shown, sameLine)
            , m_Data(data)
            , m_Callback(std::move(callback))
        {}

        /**
         * @brief   Constructor. Creates the UI text without enabler.
         */
        UICheckBox(uint64_t id, const char* text, bool& data,
            std::function<void(bool, bool)> callback = nullptr, bool shown = true, bool sameLine = false)
            : UICheckBox(id, text, data, AlwaysEnable, std::move(callback), shown, sameLine)
        {}

        /**
         * @brief   Destructor. Destroys the UI checkbox.
         */
        virtual ~UICheckBox() = default;

        /**
         * @brief   Get the data in check box.
        */
        inline bool GetData() const noexcept
        {
            return m_Data;
        }

        /**
         * @brief   Set the data in check box.
        */
        inline void SetData(bool data) noexcept
        {
            bool old = m_Data;
            m_Data = data;

            if (m_Callback)
            {
                m_Callback(data, old);
            }
        }

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;

    private:
        bool&                               m_Data;
        std::function<void(bool, bool)>     m_Callback;
    };

    /**
     * @enum UIRadioButton
     *
     * Describes of UI RadioButton element.
     *
     * @ingroup CauldronCore
    */
    class UIRadioButton : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI radio button.
         */
        UIRadioButton(uint64_t id, const char* text, const bool& enable, bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::RadioButton, text, enable, shown, sameLine)
        {}

        /**
         * @brief   Destructor. Destroys the UI radio button.
         */
        virtual ~UIRadioButton() = default;

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;
    };

    /**
     * @enum UICombo
     *
     * Describes of UI Combo element.
     *
     * @ingroup CauldronCore
    */
    class UICombo : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI combo.
         */
        template <typename T>
        UICombo(uint64_t id, const char* text, int32_t& data, T&& option, const bool& enable,
            std::function<void(int32_t, int32_t)> callback = nullptr, bool shown = true, bool sameLine = false)
            : UIElement(id, UIElementType::Combo, text, enable, shown, sameLine)
            , m_Data(data)
            , m_Option(std::forward<T>(option))
            , m_Callback(std::move(callback))
        {}

        /**
         * @brief   Constructor. Creates the UI combo without enabler.
         */
        template <typename T>
        UICombo(uint64_t id, const char* text, int32_t& data, T&& option,
            std::function<void(int32_t, int32_t)> callback = nullptr, bool shown = true, bool sameLine = false)
            : UICombo(id, text, data, std::forward<T>(option), AlwaysEnable, std::move(callback), shown, sameLine)
        {}

        /**
         * @brief   Destructor. Destroys the UI combo.
         */
        virtual ~UICombo() = default;

        /**
         * @brief   Get the data in combo.
        */
        inline int32_t GetData() const noexcept
        {
            return m_Data;
        }

        /**
         * @brief   Set the data in combo.
        */
        inline void SetData(int32_t data) noexcept
        {
            int32_t old = m_Data;
            m_Data = data;

            if (m_Callback)
            {
                m_Callback(data, old);
            }
        }

        /**
         * @brief   Set the option list in combo.
        */
        template <typename T>
        inline void SetOption(T&& option)
        {
            m_Option = std::forward<T>(option);
        }

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;

    private:
        int32_t&                                m_Data;
        std::vector<const char*>                m_Option;
        std::function<void(int32_t, int32_t)>   m_Callback;
    };

    /**
     * @enum UISlider
     *
     * Describes of UI Slider element template.
     *
     * @ingroup CauldronCore
    */
    template <typename T>
    class UISlider : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI slider.
         */
        UISlider(uint64_t id, const char* text, T& data, T minValue, T maxValue, const bool& enable,
            std::function<void(T, T)> callback = nullptr, bool shown = true, bool sameLine = false,
            const char* format = std::is_floating_point<T>::value ? "%.3f" : "%d")
            : UIElement(id, UIElementType::Slider, text, enable, shown, sameLine)
            , m_Data(data)
            , m_MinValue(minValue)
            , m_MaxValue(maxValue)
            , m_Format(format)
            , m_Callback(std::move(callback))
        {}

        /**
         * @brief   Constructor. Creates the UI slider without enabler.
         */
        UISlider(uint64_t id, const char* text, T& data, T minValue, T maxValue,
            std::function<void(T, T)> callback = nullptr, bool shown = true, bool sameLine = false,
            const char* format = std::is_floating_point<T>::value ? "%.3f" : "%d")
            : UISlider<T>(id, text, data, minValue, maxValue, AlwaysEnable, std::move(callback), shown, sameLine, format)
        {}

        /**
         * @brief   Destructor. Destroys the UI slider.
         */
        virtual ~UISlider() = default;

        /**
         * @brief   Get the data in slider.
         */
        inline T GetData() const noexcept
        {
            return m_Data;
        }

        /**
         * @brief   Set the data in slider.
         */
        inline void SetData(T data) noexcept
        {
            T old = m_Data;
            m_Data = data;

            if (m_Callback)
            {
                m_Callback(data, old);
            }
        }

        /**
         * @brief   Get the min ranger in slider.
         */
        inline void GetMin() const noexcept
        {
            return m_MinValue;
        }

        /**
         * @brief   Set the min ranger in slider.
         */
        inline void SetMin(T value) noexcept
        {
            m_MinValue = value;
            if (m_Data < value)
            {
                SetData(value);
            }
        }

        /**
         * @brief   Get the max ranger in slider.
         */
        inline void GetMax() const noexcept
        {
            return m_MaxValue;
        }

        /**
         * @brief   Set the max ranger in slider.
         */
        inline void SetMax(T value) noexcept
        {
            m_MaxValue = value;
            if (m_Data > value)
            {
                SetData(value);
            }
        }

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;

    private:
        T&                                  m_Data;
        T                                   m_MinValue = 0;
        T                                   m_MaxValue = 0;
        const char*                         m_Format;
        std::function<void(T, T)>           m_Callback;
    };

    /**
     * @enum UISeparator
     *
     * Describes of UI Separator element.
     *
     * @ingroup CauldronCore
    */
    class UISeparator : public UIElement
    {
    public:
        /**
         * @brief   Constructor. Creates the UI separator.
         */
        UISeparator(uint64_t id, bool shown = true)
            : UIElement(id, UIElementType::Separator, "", AlwaysEnable, shown, false)
        {
        }

        /**
         * @brief   Destructor. Destroys the UI separator.
         */
        virtual ~UISeparator() = default;

        /**
         * @brief   Build the UI in each UI backend.
        */
        virtual void BuildUI() override;
    };


    /**
     * @enum UISectionType
     *
     * UI section classifier. Used for internal ordering in the general UI tab
     *
     * @ingroup CauldronCore
     */
    enum class UISectionType
    {
        Framework = 0,
        Sample,
    };

    /**
     * @enum UISection
     *
     * A UISection is composed of 1 or more <c><i>UIElement</i></c>s. Represents a 
     * section of UI in the general UI tab.
     *
     * @ingroup CauldronCore
     */
    class UISection
    {
    public:
        /**
         * @brief   Constructor. Creates the UI section.
         */
        UISection(uint64_t id, const char* name, UISectionType type)
            : m_ID(id)
            , m_SectionName(name)
            , m_SectionType(type)
        {}

        /**
         * @brief   Destructor. Destroys the UI section.
         */
        ~UISection();

        /**
         * @brief   Create and registers a <c><i>UIElement</i></c>.
         */
        template <typename T, typename... U>
        inline T* RegisterUIElement(U&&... u)
        {
            return RegisterUIElementWithPriority<T>(LowestPriority, std::forward<U>(u)...);
        }

        /**
         * @brief   Registers an existed <c><i>UIElement</i></c>.
         */
        template <typename T>
        inline void RegisterUIElement(T&& element)
        {
            if (element)
            {
                m_SectionElements.emplace(element->ID(), std::forward<T>(element));
            }
        }

        /**
         * @brief   Create and registers a <c><i>UIElement</i></c> with priority.
         */
        template <typename T, typename... U>
        inline T* RegisterUIElementWithPriority(uint32_t priority, U&&... u)
        {
            T* ptr = CreateUIElementWithPriority<T>(priority, std::forward<U>(u)...);
            RegisterUIElement(ptr);
            return ptr;
        }

        /**
         * @brief   Create a <c><i>UIElement</i></c>.
         */
        template <typename T, typename... U>
        inline T* CreateUIElement(U&&... u)
        {
            return CreateUIElementWithPriority<T>(LowestPriority, std::forward<U>(u)...);
        }

        /**
         * @brief   Create a <c><i>UIElement</i></c> with priority.
         */
        template <typename T, typename... U>
        inline T* CreateUIElementWithPriority(uint32_t priority, U&&... u)
        {
            uint64_t p = priority;
            p <<= 32;
            return new T(p | m_ElementGenerator++, std::forward<U>(u)...);
        }

        /**
         * @brief   Unregisters an existed <c><i>UIElement</i></c>. This will make the <c><i>UIElement</i></c> stop rendering.
         */
        template <typename T>
        inline bool UnregisterUIElement(T&& element)
        {
            bool result = false;
            if (element)
            {
                std::map<uint64_t, UIElement*>::const_iterator itor = m_SectionElements.find(element->ID());
                {
                    if (itor != m_SectionElements.cend() &&
                        itor->second == element)
                    {
                        m_SectionElements.erase(itor);
                        result = true;
                    }
                }
            }
            return true;
        }

        /**
         * @brief   Get the section description.
         */
        inline const char* GetSectionName() const noexcept
        {
            return m_SectionName.c_str();
        }

        /**
         * @brief   Get the <c><i>UIElement</i></c>s in this section.
         */
        inline const std::map<uint64_t, UIElement*>& GetElements() const noexcept
        {
            return m_SectionElements;
        }

        /**
         * @brief   Get the ID of this section. ID represents the priority, lower ID means higher priority.
         */
        inline uint64_t ID() const noexcept
        {
            return m_ID;
        }

        /**
         * @brief   Check the section is shown.
        */
        inline bool Shown() const noexcept
        {
            return m_Shown;
        }

        /**
         * @brief   Show or hide the element.
        */
        inline void Show(bool show) noexcept
        {
            m_Shown = show;
        }

        /**
         * @brief   Release UISection.
         */
        inline void Release()
        {
            delete this;
        }

    private:
        uint64_t            m_ID = 0;                                   ///< Section id
        std::string         m_SectionName = "";                         ///< Section name
        const UISectionType m_SectionType = UISectionType::Framework;   ///< <c><i>UISectionType</i></c>
        bool                m_Shown       = true;

        std::map<uint64_t, UIElement*> m_SectionElements;  // Section ui elements
        
        uint32_t m_ElementGenerator = 0;     // Element ID in this section
        static const uint32_t LowestPriority = std::numeric_limits<uint32_t>::max();
    };



    class UIBackend;

    /**
     * @enum UIManager
     *
     * The UIManager instance is responsible for setting up the individual ui elements for the General UI tab.
     * This is the main tab of the graphics framework and is used to convey the bulk of the sample information.
     *
     * @ingroup CauldronCore
     */
    class UIManager
    {
    public:

        /**
         * @brief   Constructor. Creates the UI backend.
         */
        UIManager();

        /**
         * @brief   Destructor. Destroys the UI backend.
         */
        virtual ~UIManager();

        /**
         * @brief   Updates the UI for the frame, going through the <c><i>UIBackend</i></c>.
         */
        void Update(double deltaTime);

        /**
         * @brief   Message handler for the UIManager. Calls through to the <c><i>UIBackend</i></c>.
         */
        bool UIBackendMessageHandler(void* pMessage);

        /**
         * @brief   Registers a new <c><i>UISection</i></c>.
         */
        UISection* RegisterUIElements(const char* name, UISectionType type = UISectionType::Framework);

        /**
         * @brief   Create a new <c><i>UISection</i></c>.
         */
        UISection* CreateUIElements(const char* name, UISectionType type = UISectionType::Framework);

        /**
         * @brief   Registers a existed <c><i>UISection</i></c>.
         */
        template <typename T>
        void RegisterUIElements(T&& uiSectoin)
        {
            CauldronAssert(ASSERT_CRITICAL, !m_ProcessingUI, L"UI element stack cannot be updated during UI update cycle.");
            
            if (uiSectoin)
            {
                m_UIGeneralLayout.emplace(uiSectoin->ID(), std::forward<T>(uiSectoin));
            }
        }

        /**
         * @brief   Unregisters a <c><i>UISection</i></c>. This will make the <c><i>UISection</i></c> stop rendering.
         */
        template <typename T>
        bool UnregisterUIElements(T&& uiSection)
        {
            CauldronAssert(ASSERT_CRITICAL, !m_ProcessingUI, L"UI element stack cannot be updated during UI update cycle.");

            bool result = false;
            if (uiSection)
            {
                std::map<uint64_t, UISection*>::const_iterator itor = m_UIGeneralLayout.find(uiSection->ID());
                {
                    if (itor != m_UIGeneralLayout.cend() && 
                        itor->second == uiSection)
                    {
                        m_UIGeneralLayout.erase(itor);
                        result = true;
                    }
                }
            }
            return true;
        }

        /**
         * @brief   Gets all <c><i>UISection</i></c> elements that make up the general layout tab.
         */
        const std::map<uint64_t, UISection*>& GetGeneralLayout() const
        {
            return m_UIGeneralLayout;
        }

    private:

        // No Copy, No Move
        NO_COPY(UIManager);
        NO_MOVE(UIManager);

    private:

        UIBackend*                                      m_pUIBackEnd = nullptr;
        std::map<uint64_t, UISection*>  m_UIGeneralLayout;  // Layout for the general tab in the UI (all elements go here under various headings)
        std::atomic_bool                                m_ProcessingUI = false;
        uint32_t                                        m_SectionIDGenerator = 0;
        static const uint32_t                           LowestPriority       = std::numeric_limits<uint32_t>::max();
    };

} // namespace cauldron
