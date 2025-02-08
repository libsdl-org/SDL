-- This code adapted from https://gitlab.com/saalen/highlight/-/wikis/Plug-Ins

-- first add a description of what the plug-in does
Description="Add wiki.libsdl.org reference links to HTML, LaTeX or RTF output"

-- define the plugin categories (ie. supported output formats; languages)
Categories = { "c", "c++" }

-- the syntaxUpdate function contains code related to syntax recognition
function syntaxUpdate(desc)

  -- if the current file is not C/C++ file we exit
  if desc~="C and C++" then
     return
  end

  -- this function returns a qt-project reference link of the given token
  function getURL(token)
     -- generate the URL
     url='https://wiki.libsdl.org/SDL3/'.. token

     -- embed the URL in a hyperlink according to the output format
     -- first HTML, then LaTeX and RTF
     if (HL_OUTPUT== HL_FORMAT_HTML or HL_OUTPUT == HL_FORMAT_XHTML) then
         return '<a class="hl" target="new" href="'
                .. url .. '">'.. token .. '</a>'
     elseif (HL_OUTPUT == HL_FORMAT_LATEX) then
         return '\\href{'..url..'}{'..token..'}'
     elseif (HL_OUTPUT == HL_FORMAT_RTF) then
         return '{{\\field{\\*\\fldinst HYPERLINK "'
                ..url..'" }{\\fldrslt\\ul\\ulc0 '..token..'}}}'
     end
   end

  -- the Decorate function will be invoked for every recognized token
  function Decorate(token, state)

    -- we are only interested in keywords, preprocessor or default items
    if (state ~= HL_STANDARD and state ~= HL_KEYWORD and
        state ~=HL_PREPROC) then
      return
    end

    -- SDL keywords start with SDL_
    -- if this pattern applies to the token, we return the URL
    -- if we return nothing, the token is outputted as is
    if ( (token == "Uint8") or (token == "Uint16") or (token == "Uint32") or (token == "Uint64") or
         (token == "Sint8") or (token == "Sint16") or (token == "Sint32") or (token == "Sint64") or
         (string.find(token, "SDL_") == 1) ) then
      return getURL(token)
    end

  end
end

-- the themeUpdate function contains code related to the theme
function themeUpdate(desc)
  -- the Injections table can be used to add style information to the theme

  -- HTML: we add additional CSS style information to beautify hyperlinks,
  -- they should have the same color as their surrounding tags
  if (HL_OUTPUT == HL_FORMAT_HTML or HL_OUTPUT == HL_FORMAT_XHTML) then
    Injections[#Injections+1]=
      "a.hl, a.hl:visited {color:inherit;font-weight:inherit;text-decoration:none}"

  -- LaTeX: hyperlinks require the hyperref package, so we add this here
  -- the colorlinks and pdfborderstyle options remove ugly boxes in the output
  elseif (HL_OUTPUT==HL_FORMAT_LATEX) then
    Injections[#Injections+1]=
      "\\usepackage[colorlinks=false, pdfborderstyle={/S/U/W 1}]{hyperref}"
  end
end

-- let highlight load the chunks
Plugins={
  { Type="lang", Chunk=syntaxUpdate },
  { Type="theme", Chunk=themeUpdate },
}

