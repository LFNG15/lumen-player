#ifndef LANG_H
#define LANG_H

#include <QString>
#include <QHash>

// Lightweight UI translation layer. Portuguese is the app's source language:
// every user-visible string is written in Portuguese and used as the lookup
// key; Lang::tr() returns the English text when the language setting is "en"
// and the key itself otherwise. Unknown keys fall back to Portuguese, so a
// missing translation never breaks the UI.
//
// The active language is loaded from QSettings ("language") at startup in
// main.cpp; switching languages restarts the app, like switching themes.
namespace Lang {

inline QString &activeLang() {
    static QString lang = QStringLiteral("pt");
    return lang;
}

inline void setActiveLang(const QString &lang) { activeLang() = lang; }
inline bool isEnglish() { return activeLang() == QLatin1String("en"); }

inline QString tr(const QString &pt) {
    if (!isEnglish()) return pt;

    static const QHash<QString, QString> en = {
        // Common
        {"Cancelar", "Cancel"},
        {"Salvar", "Save"},
        {"Criar", "Create"},
        {"Remover", "Remove"},
        {"Importar", "Import"},
        {"Fechar", "Close"},
        {"Baixar", "Download"},
        {"Alterar", "Change"},
        {"Desconhecido", "Unknown"},
        {"Curtidas", "Liked Songs"},
        {"Músicas", "Songs"},
        {"Fila", "Queue"},
        {"Início", "Home"},
        {"Adicionar", "Add"},
        {"Adicionado à fila", "Added to queue"},
        {"Adicionar à fila", "Add to queue"},
        {"%1 faixa%2", "%1 track%2"},
        {"Músicas avulsas", "Standalone songs"},
        {"Idioma", "Language"},

        // Main window / sidebar
        {"SUAS PLAYLISTS", "YOUR PLAYLISTS"},
        {"Nenhuma playlist", "No playlists"},
        {"Buscar playlists", "Search playlists"},
        {"Ordenar e exibir", "Sort and view"},
        {"Ordenar por", "Sort by"},
        {"Alfabética", "Alphabetical"},
        {"Exibir como", "View as"},
        {"Compacta", "Compact"},
        {"Lista", "List"},
        {"Grade", "Grid"},
        {"%1 faixa%2 na biblioteca", "%1 track%2 in your library"},
        {"Escolher tema", "Choose theme"},
        {"Recolher menu", "Collapse menu"},
        {"Expandir menu", "Expand menu"},
        {"Escolher Tema", "Choose Theme"},
        {"Paleta de Cores", "Color Palette"},
        {"Editar Música", "Edit Song"},
        {"Nome da música", "Song name"},
        {"Artista", "Artist"},
        {"Excluir Música", "Delete Song"},
        {"Excluir \"%1\"?", "Delete \"%1\"?"},
        {"A música será removida da biblioteca.", "The song will be removed from your library."},
        {"O que você quer ouvir?", "What do you want to play?"},
        {"Português", "Portuguese"},
        {"English", "English"},

        // Theme names
        {"Vinil Quente", "Warm Vinyl"},
        {"Oceano", "Ocean"},
        {"Floresta", "Forest"},
        {"Roxo Noturno", "Night Purple"},
        {"Cinza Moderno", "Modern Gray"},

        // Player bar
        {"Adicione músicas para começar a ouvir", "Add songs to start listening"},
        {"Fila de reprodução", "Playback queue"},
        {"Silenciar", "Mute"},
        {"Ativar som", "Unmute"},

        // Home
        {"Bom dia", "Good morning"},
        {"Boa tarde", "Good afternoon"},
        {"Boa noite", "Good evening"},
        {"Sua biblioteca está vazia", "Your library is empty"},
        {"Adicione seus arquivos de áudio para começar", "Add your audio files to get started"},
        {"Adicionar Músicas", "Add Songs"},
        {"Recentes", "Recents"},
        {"Tocadas recentemente", "Recently played"},
        {"Adicionadas recentemente", "Recently added"},
        {"Biblioteca Completa", "Full Library"},
        {"Ir para a playlist \"%1\"", "Go to playlist \"%1\""},
        {"Editar música", "Edit song"},
        {"Excluir música", "Delete song"},

        // Search
        {"Digite algo na busca para encontrar músicas e playlists",
         "Type in the search bar to find songs and playlists"},
        {"Resultados para “%1”", "Results for “%1”"},
        {"Nenhum resultado para “%1”\nVerifique a escrita ou tente outras palavras",
         "No results for “%1”\nCheck the spelling or try different keywords"},

        // Playlists page
        {"Nova Playlist", "New Playlist"},
        {"Nenhuma playlist ainda\nCrie uma playlist ou adicione músicas",
         "No playlists yet\nCreate a playlist or add songs"},
        {"Nome da playlist", "Playlist name"},
        {"Ex: Minha Playlist", "E.g.: My Playlist"},
        {"Cores da capa:", "Cover colors:"},
        {"Cor 1", "Color 1"},
        {"Cor 2", "Color 2"},
        {"Escolher imagem", "Choose image"},
        {"Escolher imagem da capa", "Choose cover image"},
        {"Imagens (*.png *.jpg *.jpeg *.bmp *.webp)", "Images (*.png *.jpg *.jpeg *.bmp *.webp)"},
        {"Renomear", "Rename"},
        {"Editar capa", "Edit cover"},
        {"Renomear Playlist", "Rename Playlist"},
        {"Editar Capa", "Edit Cover"},
        {"Escolha as cores do gradiente:", "Choose the gradient colors:"},
        {"Voltar a usar o gradiente de cores", "Go back to the gradient cover"},
        {"Excluir Playlist", "Delete Playlist"},
        {"Excluir playlist", "Delete playlist"},
        {"Excluir a playlist \"%1\"?", "Delete playlist \"%1\"?"},
        {"As músicas não serão apagadas — ficarão como músicas avulsas.",
         "The songs won't be deleted — they'll remain as standalone songs."},

        // Playlist detail
        {"Buscar na playlist", "Search in playlist"},
        {"Ordenar", "Sort"},
        {"Personalizada", "Custom"},
        {"Título", "Title"},
        {"Mais antigas", "Oldest first"},
        {"Duração", "Duration"},
        {"MÚSICAS AVULSAS", "STANDALONE SONGS"},
        {"PLAYLIST", "PLAYLIST"},
        {"COLEÇÃO", "COLLECTION"},
        {"Editar playlist", "Edit playlist"},
        {"Ver imagem", "View image"},
        {"Editar Playlist", "Edit Playlist"},
        {"Imagem da capa:", "Cover image:"},
        {"Adicionar à playlist", "Add to playlist"},
        {"Mover para outra playlist", "Move to another playlist"},
        {"Remover da playlist", "Remove from playlist"},
        {"Nenhuma playlist disponível", "No playlists available"},
        {"%1 faixa%2%3", "%1 track%2%3"},

        // Liked page
        {"Tocar curtidas", "Play liked songs"},
        {"Nenhuma música curtida ainda", "No liked songs yet"},

        // Queue page
        {"A fila está vazia\nReproduza uma música ou adicione faixas à fila",
         "The queue is empty\nPlay a song or add tracks to the queue"},
        {"TOCANDO AGORA", "NOW PLAYING"},
        {"PRÓXIMAS NA FILA", "NEXT IN QUEUE"},
        {"A SEGUIR", "UP NEXT"},
        {"Remover da fila", "Remove from queue"},

        // Add music page
        {"Inserção de Músicas", "Add Music"},
        {"Arraste seus arquivos de áudio ou clique para selecionar",
         "Drag your audio files here or click to select"},
        {"Salvar em: ", "Save to: "},
        {"Escolher pasta de downloads", "Choose downloads folder"},
        {"Converter playlist de streaming", "Convert streaming playlist"},
        {"Cole o link de uma playlist do Spotify ou do YouTube para trazê-la para o Lumen Music.",
         "Paste a Spotify or YouTube playlist link to bring it into Lumen Music."},
        {"Use o link de uma playlist do Spotify (open.spotify.com/playlist/...) ou do YouTube (com \"list=\").",
         "Use a Spotify playlist link (open.spotify.com/playlist/...) or a YouTube one (with \"list=\")."},
        {"Clique ou arraste arquivos de áudio", "Click or drag audio files"},
        {"Solte os arquivos aqui", "Drop the files here"},
        {"Procurar Arquivos", "Browse Files"},
        {"Selecionar Músicas", "Select Songs"},
        {"Áudio (*.opus *.webm *.m4a *.mp3 *.ogg *.oga *.flac *.wav *.aac)",
         "Audio (*.opus *.webm *.m4a *.mp3 *.ogg *.oga *.flac *.wav *.aac)"},
        {"%1 arquivo%2 selecionado%2", "%1 file%2 selected"},
        {"Título da música", "Song title"},
        {"Nome do artista", "Artist name"},
        {"Adicionar à Biblioteca", "Add to Library"},
        {"Adicionar %1 Música%2", "Add %1 Song%2"},
        {"Sem playlist", "No playlist"},
        {"Nova playlist...", "New playlist..."},
        {"PLAYLIST DE DESTINO", "DESTINATION PLAYLIST"},
        {"Conectando...", "Connecting..."},
        {"Baixando... %1%", "Downloading... %1%"},
        {"Baixando...", "Downloading..."},
        {"Convertendo para Opus...", "Converting to Opus..."},
        {"Use um link do YouTube (youtube.com ou youtu.be).",
         "Use a YouTube link (youtube.com or youtu.be)."},
        {"yt-dlp não encontrado. Instale com: pip install yt-dlp",
         "yt-dlp not found. Install it with: pip install yt-dlp"},
        {"yt-dlp não encontrado. Verifique sua pasta de instalação.",
         "yt-dlp not found. Check your installation folder."},
        {"Verifique o link ou tente novamente.", "Check the link or try again."},
        {"Erro: ", "Error: "},
        {"Download concluído, mas nenhum arquivo de áudio encontrado.",
         "Download finished, but no audio file was found."},
        {"Concluído! %1 arquivo%2 pronto%2 para adicionar.", "Done! %1 file%2 ready to add."},

        // Import playlist dialog
        {"Importar Playlist", "Import Playlist"},
        {"Importar playlist de streaming", "Import streaming playlist"},
        {"Buscando informações da playlist...", "Fetching playlist info..."},
        {"Nome da playlist no Lumen Music", "Playlist name in Lumen Music"},
        {"Não foi possível acessar o Spotify: ", "Could not reach Spotify: "},
        {"Não foi possível ler a playlist do Spotify. Verifique se o link é público e tente novamente.",
         "Could not read the Spotify playlist. Make sure the link is public and try again."},
        {"Não foi possível listar a playlist do YouTube. Verifique o link.",
         "Could not list the YouTube playlist. Check the link."},
        {"Playlist importada", "Imported playlist"},
        {"Nenhuma música encontrada nessa playlist.", "No songs found in that playlist."},
        {"Buscando correspondências no YouTube... (%1 de %2)",
         "Matching songs on YouTube... (%1 of %2)"},
        {"Revise a lista: %1 música%2 em laranja podem estar erradas. Marque para aprovar e desmarque para reprovar.",
         "Review the list: %1 song%2 in orange may be wrong. Check to approve, uncheck to reject."},
        {"Tudo pronto. Desmarque alguma música para não importá-la.",
         "All set. Uncheck any song you don't want to import."},
        {"Nenhuma correspondência encontrada", "No match found"},
        {"Nenhuma música selecionada para importar.", "No songs selected to import."},
        {"Baixando %1 de %2: %3", "Downloading %1 of %2: %3"},
        {"Concluído! %1 de %2 música%3 importada%3 para \"%4\".",
         "Done! %1 of %2 song%3 imported to \"%4\"."},
    };

    return en.value(pt, pt);
}

}  // namespace Lang

#endif // LANG_H
