#include "i18n.h"

#include <QLabel>
#include <QAbstractButton>
#include <QLineEdit>
#include <QSettings>
#include <algorithm>

// Keep the Portuguese→English dictionary in one place (legacy lang.h keys).
// LanguageManager is the live owner; Lang::tr forwards here.

namespace lumen::design {

namespace {

const QHash<QString, QString> &enDict()
{
    static const QHash<QString, QString> en = {
        {QStringLiteral("Cancelar"), QStringLiteral("Cancel")},
        {QStringLiteral("Salvar"), QStringLiteral("Save")},
        {QStringLiteral("Criar"), QStringLiteral("Create")},
        {QStringLiteral("Remover"), QStringLiteral("Remove")},
        {QStringLiteral("Importar"), QStringLiteral("Import")},
        {QStringLiteral("Fechar"), QStringLiteral("Close")},
        {QStringLiteral("Baixar"), QStringLiteral("Download")},
        {QStringLiteral("Alterar"), QStringLiteral("Change")},
        {QStringLiteral("Desconhecido"), QStringLiteral("Unknown")},
        {QStringLiteral("Curtidas"), QStringLiteral("Liked Songs")},
        {QStringLiteral("Músicas"), QStringLiteral("Songs")},
        {QStringLiteral("Fila"), QStringLiteral("Queue")},
        {QStringLiteral("Início"), QStringLiteral("Home")},
        {QStringLiteral("Adicionar"), QStringLiteral("Add")},
        {QStringLiteral("Adicionado à fila"), QStringLiteral("Added to queue")},
        {QStringLiteral("Adicionar à fila"), QStringLiteral("Add to queue")},
        {QStringLiteral("%1 faixa%2"), QStringLiteral("%1 track%2")},
        {QStringLiteral("Músicas avulsas"), QStringLiteral("Standalone songs")},
        {QStringLiteral("Idioma"), QStringLiteral("Language")},
        {QStringLiteral("SUAS PLAYLISTS"), QStringLiteral("YOUR PLAYLISTS")},
        {QStringLiteral("Nenhuma playlist"), QStringLiteral("No playlists")},
        {QStringLiteral("Buscar playlists"), QStringLiteral("Search playlists")},
        {QStringLiteral("Ordenar e exibir"), QStringLiteral("Sort and view")},
        {QStringLiteral("Ordenar por"), QStringLiteral("Sort by")},
        {QStringLiteral("Alfabética"), QStringLiteral("Alphabetical")},
        {QStringLiteral("Exibir como"), QStringLiteral("View as")},
        {QStringLiteral("Compacta"), QStringLiteral("Compact")},
        {QStringLiteral("Lista"), QStringLiteral("List")},
        {QStringLiteral("Grade"), QStringLiteral("Grid")},
        {QStringLiteral("%1 faixa%2 na biblioteca"), QStringLiteral("%1 track%2 in your library")},
        {QStringLiteral("Escolher tema"), QStringLiteral("Choose theme")},
        {QStringLiteral("Recolher menu"), QStringLiteral("Collapse menu")},
        {QStringLiteral("Expandir menu"), QStringLiteral("Expand menu")},
        {QStringLiteral("Escolher Tema"), QStringLiteral("Choose Theme")},
        {QStringLiteral("Paleta de Cores"), QStringLiteral("Color Palette")},
        {QStringLiteral("Editar Música"), QStringLiteral("Edit Song")},
        {QStringLiteral("Nome da música"), QStringLiteral("Song name")},
        {QStringLiteral("Artista"), QStringLiteral("Artist")},
        {QStringLiteral("Excluir Música"), QStringLiteral("Delete Song")},
        {QStringLiteral("Excluir \"%1\"?"), QStringLiteral("Delete \"%1\"?")},
        {QStringLiteral("A música será removida da biblioteca."), QStringLiteral("The song will be removed from your library.")},
        {QStringLiteral("O que você quer ouvir?"), QStringLiteral("What do you want to play?")},
        {QStringLiteral("Português"), QStringLiteral("Portuguese")},
        {QStringLiteral("English"), QStringLiteral("English")},
        {QStringLiteral("Vinil Quente"), QStringLiteral("Warm Vinyl")},
        {QStringLiteral("Oceano"), QStringLiteral("Ocean")},
        {QStringLiteral("Floresta"), QStringLiteral("Forest")},
        {QStringLiteral("Roxo Noturno"), QStringLiteral("Night Purple")},
        {QStringLiteral("Cinza Moderno"), QStringLiteral("Modern Gray")},
        {QStringLiteral("Adicione músicas para começar a ouvir"), QStringLiteral("Add songs to start listening")},
        {QStringLiteral("Fila de reprodução"), QStringLiteral("Playback queue")},
        {QStringLiteral("Silenciar"), QStringLiteral("Mute")},
        {QStringLiteral("Ativar som"), QStringLiteral("Unmute")},
        {QStringLiteral("Bom dia"), QStringLiteral("Good morning")},
        {QStringLiteral("Boa tarde"), QStringLiteral("Good afternoon")},
        {QStringLiteral("Boa noite"), QStringLiteral("Good evening")},
        {QStringLiteral("Sua biblioteca está vazia"), QStringLiteral("Your library is empty")},
        {QStringLiteral("Adicione seus arquivos de áudio para começar"), QStringLiteral("Add your audio files to get started")},
        {QStringLiteral("Adicionar Músicas"), QStringLiteral("Add Songs")},
        {QStringLiteral("Recentes"), QStringLiteral("Recents")},
        {QStringLiteral("Tocadas recentemente"), QStringLiteral("Recently played")},
        {QStringLiteral("Adicionadas recentemente"), QStringLiteral("Recently added")},
        {QStringLiteral("Biblioteca Completa"), QStringLiteral("Full Library")},
        {QStringLiteral("Ir para a playlist \"%1\""), QStringLiteral("Go to playlist \"%1\"")},
        {QStringLiteral("Editar música"), QStringLiteral("Edit song")},
        {QStringLiteral("Excluir música"), QStringLiteral("Delete song")},
        {QStringLiteral("Digite algo na busca para encontrar músicas e playlists"), QStringLiteral("Type in the search bar to find songs and playlists")},
        {QStringLiteral("Resultados para “%1”"), QStringLiteral("Results for “%1”")},
        {QStringLiteral("Nenhum resultado para “%1”\nVerifique a escrita ou tente outras palavras"), QStringLiteral("No results for “%1”\nCheck the spelling or try different keywords")},
        {QStringLiteral("Nova Playlist"), QStringLiteral("New Playlist")},
        {QStringLiteral("Nenhuma playlist ainda\nCrie uma playlist ou adicione músicas"), QStringLiteral("No playlists yet\nCreate a playlist or add songs")},
        {QStringLiteral("Nome da playlist"), QStringLiteral("Playlist name")},
        {QStringLiteral("Ex: Minha Playlist"), QStringLiteral("E.g.: My Playlist")},
        {QStringLiteral("Cores da capa:"), QStringLiteral("Cover colors:")},
        {QStringLiteral("Cor 1"), QStringLiteral("Color 1")},
        {QStringLiteral("Cor 2"), QStringLiteral("Color 2")},
        {QStringLiteral("Escolher imagem"), QStringLiteral("Choose image")},
        {QStringLiteral("Escolher imagem da capa"), QStringLiteral("Choose cover image")},
        {QStringLiteral("Imagens (*.png *.jpg *.jpeg *.bmp *.webp)"), QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)")},
        {QStringLiteral("Renomear"), QStringLiteral("Rename")},
        {QStringLiteral("Editar capa"), QStringLiteral("Edit cover")},
        {QStringLiteral("Renomear Playlist"), QStringLiteral("Rename Playlist")},
        {QStringLiteral("Editar Capa"), QStringLiteral("Edit Cover")},
        {QStringLiteral("Escolha as cores do gradiente:"), QStringLiteral("Choose the gradient colors:")},
        {QStringLiteral("Voltar a usar o gradiente de cores"), QStringLiteral("Go back to the gradient cover")},
        {QStringLiteral("Excluir Playlist"), QStringLiteral("Delete Playlist")},
        {QStringLiteral("Excluir playlist"), QStringLiteral("Delete playlist")},
        {QStringLiteral("Excluir a playlist \"%1\"?"), QStringLiteral("Delete playlist \"%1\"?")},
        {QStringLiteral("As músicas não serão apagadas — ficarão como músicas avulsas."), QStringLiteral("The songs won't be deleted — they'll remain as standalone songs.")},
        {QStringLiteral("Buscar na playlist"), QStringLiteral("Search in playlist")},
        {QStringLiteral("Ordenar"), QStringLiteral("Sort")},
        {QStringLiteral("Personalizada"), QStringLiteral("Custom")},
        {QStringLiteral("Título"), QStringLiteral("Title")},
        {QStringLiteral("Mais antigas"), QStringLiteral("Oldest first")},
        {QStringLiteral("Duração"), QStringLiteral("Duration")},
        {QStringLiteral("MÚSICAS AVULSAS"), QStringLiteral("STANDALONE SONGS")},
        {QStringLiteral("PLAYLIST"), QStringLiteral("PLAYLIST")},
        {QStringLiteral("COLEÇÃO"), QStringLiteral("COLLECTION")},
        {QStringLiteral("Editar playlist"), QStringLiteral("Edit playlist")},
        {QStringLiteral("Ver imagem"), QStringLiteral("View image")},
        {QStringLiteral("Editar Playlist"), QStringLiteral("Edit Playlist")},
        {QStringLiteral("Imagem da capa:"), QStringLiteral("Cover image:")},
        {QStringLiteral("Adicionar à playlist"), QStringLiteral("Add to playlist")},
        {QStringLiteral("Mover para outra playlist"), QStringLiteral("Move to another playlist")},
        {QStringLiteral("Remover da playlist"), QStringLiteral("Remove from playlist")},
        {QStringLiteral("Nenhuma playlist disponível"), QStringLiteral("No playlists available")},
        {QStringLiteral("%1 faixa%2%3"), QStringLiteral("%1 track%2%3")},
        {QStringLiteral("Tocar curtidas"), QStringLiteral("Play liked songs")},
        {QStringLiteral("Nenhuma música curtida ainda"), QStringLiteral("No liked songs yet")},
        {QStringLiteral("A fila está vazia\nReproduza uma música ou adicione faixas à fila"), QStringLiteral("The queue is empty\nPlay a song or add tracks to the queue")},
        {QStringLiteral("TOCANDO AGORA"), QStringLiteral("NOW PLAYING")},
        {QStringLiteral("PRÓXIMAS NA FILA"), QStringLiteral("NEXT IN QUEUE")},
        {QStringLiteral("A SEGUIR"), QStringLiteral("UP NEXT")},
        {QStringLiteral("Remover da fila"), QStringLiteral("Remove from queue")},
        {QStringLiteral("Inserção de Músicas"), QStringLiteral("Add Music")},
        {QStringLiteral("Arraste seus arquivos de áudio ou clique para selecionar"), QStringLiteral("Drag your audio files here or click to select")},
        {QStringLiteral("Salvar em: "), QStringLiteral("Save to: ")},
        {QStringLiteral("Escolher pasta de downloads"), QStringLiteral("Choose downloads folder")},
        {QStringLiteral("Converter playlist de streaming"), QStringLiteral("Convert streaming playlist")},
        {QStringLiteral("Cole o link de uma playlist do Spotify ou do YouTube para trazê-la para o Lumen Music."), QStringLiteral("Paste a Spotify or YouTube playlist link to bring it into Lumen Music.")},
        {QStringLiteral("Use o link de uma playlist do Spotify (open.spotify.com/playlist/...) ou do YouTube (com \"list=\")."), QStringLiteral("Use a Spotify playlist link (open.spotify.com/playlist/...) or a YouTube one (with \"list=\").")},
        {QStringLiteral("Clique ou arraste arquivos de áudio"), QStringLiteral("Click or drag audio files")},
        {QStringLiteral("Solte os arquivos aqui"), QStringLiteral("Drop the files here")},
        {QStringLiteral("Procurar Arquivos"), QStringLiteral("Browse Files")},
        {QStringLiteral("Selecionar Músicas"), QStringLiteral("Select Songs")},
        {QStringLiteral("Áudio (*.opus *.webm *.m4a *.mp3 *.ogg *.oga *.flac *.wav *.aac)"), QStringLiteral("Audio (*.opus *.webm *.m4a *.mp3 *.ogg *.oga *.flac *.wav *.aac)")},
        {QStringLiteral("%1 arquivo%2 selecionado%2"), QStringLiteral("%1 file%2 selected")},
        {QStringLiteral("Título da música"), QStringLiteral("Song title")},
        {QStringLiteral("Nome do artista"), QStringLiteral("Artist name")},
        {QStringLiteral("Adicionar à Biblioteca"), QStringLiteral("Add to Library")},
        {QStringLiteral("Adicionar %1 Música%2"), QStringLiteral("Add %1 Song%2")},
        {QStringLiteral("Sem playlist"), QStringLiteral("No playlist")},
        {QStringLiteral("Nova playlist..."), QStringLiteral("New playlist...")},
        {QStringLiteral("PLAYLIST DE DESTINO"), QStringLiteral("DESTINATION PLAYLIST")},
        {QStringLiteral("Conectando..."), QStringLiteral("Connecting...")},
        {QStringLiteral("Baixando... %1%"), QStringLiteral("Downloading... %1%")},
        {QStringLiteral("Baixando..."), QStringLiteral("Downloading...")},
        {QStringLiteral("Convertendo para Opus..."), QStringLiteral("Converting to Opus...")},
        {QStringLiteral("Use um link do YouTube (youtube.com ou youtu.be)."), QStringLiteral("Use a YouTube link (youtube.com or youtu.be).")},
        {QStringLiteral("yt-dlp não encontrado. Instale com: pip install yt-dlp"), QStringLiteral("yt-dlp not found. Install it with: pip install yt-dlp")},
        {QStringLiteral("yt-dlp não encontrado. Verifique sua pasta de instalação."), QStringLiteral("yt-dlp not found. Check your installation folder.")},
        {QStringLiteral("Verifique o link ou tente novamente."), QStringLiteral("Check the link or try again.")},
        {QStringLiteral("Erro: "), QStringLiteral("Error: ")},
        {QStringLiteral("Download concluído, mas nenhum arquivo de áudio encontrado."), QStringLiteral("Download finished, but no audio file was found.")},
        {QStringLiteral("Concluído! %1 arquivo%2 pronto%2 para adicionar."), QStringLiteral("Done! %1 file%2 ready to add.")},
        {QStringLiteral("Importar Playlist"), QStringLiteral("Import Playlist")},
        {QStringLiteral("Importar playlist de streaming"), QStringLiteral("Import streaming playlist")},
        {QStringLiteral("Buscando informações da playlist..."), QStringLiteral("Fetching playlist info...")},
        {QStringLiteral("Nome da playlist no Lumen Music"), QStringLiteral("Playlist name in Lumen Music")},
        {QStringLiteral("Não foi possível acessar o Spotify: "), QStringLiteral("Could not reach Spotify: ")},
        {QStringLiteral("Não foi possível ler a playlist do Spotify. Verifique se o link é público e tente novamente."), QStringLiteral("Could not read the Spotify playlist. Make sure the link is public and try again.")},
        {QStringLiteral("Não foi possível listar a playlist do YouTube. Verifique o link."), QStringLiteral("Could not list the YouTube playlist. Check the link.")},
        {QStringLiteral("Playlist importada"), QStringLiteral("Imported playlist")},
        {QStringLiteral("Nenhuma música encontrada nessa playlist."), QStringLiteral("No songs found in that playlist.")},
        {QStringLiteral("Buscando correspondências no YouTube... (%1 de %2)"), QStringLiteral("Matching songs on YouTube... (%1 of %2)")},
        {QStringLiteral("Revise a lista: %1 música%2 em laranja podem estar erradas. Marque para aprovar e desmarque para reprovar."), QStringLiteral("Review the list: %1 song%2 in orange may be wrong. Check to approve, uncheck to reject.")},
        {QStringLiteral("Tudo pronto. Desmarque alguma música para não importá-la."), QStringLiteral("All set. Uncheck any song you don't want to import.")},
        {QStringLiteral("Nenhuma correspondência encontrada"), QStringLiteral("No match found")},
        {QStringLiteral("Nenhuma música selecionada para importar."), QStringLiteral("No songs selected to import.")},
        {QStringLiteral("Baixando %1 de %2: %3"), QStringLiteral("Downloading %1 of %2: %3")},
        {QStringLiteral("Concluído! %1 de %2 música%3 importada%3 para \"%4\"."), QStringLiteral("Done! %1 of %2 song%3 imported to \"%4\".")},
        {QStringLiteral("Modo"), QStringLiteral("Mode")},
        {QStringLiteral("Escuro"), QStringLiteral("Dark")},
        {QStringLiteral("Claro"), QStringLiteral("Light")},
        {QStringLiteral("Alto contraste"), QStringLiteral("High contrast")},
        {QStringLiteral("Densidade"), QStringLiteral("Density")},
        {QStringLiteral("Confortável"), QStringLiteral("Comfortable")},
        {QStringLiteral("Reduzir movimento"), QStringLiteral("Reduce motion")},
        {QStringLiteral("«%1» foi adicionada a %2. O arquivo não foi duplicado — ele permanece na pasta de %3."),
         QStringLiteral("«%1» was added to %2. The file was not duplicated — it remains in %3's folder.")},
        {QStringLiteral("«%1» foi adicionada a %2. O arquivo permanece na pasta geral do Lumen Music."),
         QStringLiteral("«%1» was added to %2. The file remains in the general Lumen Music folder.")},
        {QStringLiteral("Tocar"), QStringLiteral("Play")},
        {QStringLiteral("Curtir"), QStringLiteral("Like")},
        {QStringLiteral("Descurtir"), QStringLiteral("Unlike")},
        {QStringLiteral("Excluir"), QStringLiteral("Delete")},
        {QStringLiteral("Tocar curtidas"), QStringLiteral("Play liked songs")},
        {QStringLiteral("Repetir"), QStringLiteral("Repeat")},
        {QStringLiteral("Repetir todas"), QStringLiteral("Repeat all")},
        {QStringLiteral("Repetir uma"), QStringLiteral("Repeat one")},
        {QStringLiteral("Mostrar Lumen Music"), QStringLiteral("Show Lumen Music")},
        {QStringLiteral("Sair"), QStringLiteral("Quit")},
        {QStringLiteral("Próxima"), QStringLiteral("Next")},
        {QStringLiteral("Anterior"), QStringLiteral("Previous")},
        {QStringLiteral("Play / Pause"), QStringLiteral("Play / Pause")},
    };
    return en;
}

} // namespace

LanguageManager &LanguageManager::instance()
{
    static LanguageManager mgr;
    return mgr;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent)
{
}

void LanguageManager::loadFromSettings()
{
    QSettings s;
    m_lang = s.value(QStringLiteral("language"), QStringLiteral("pt")).toString();
    if (m_lang != QLatin1String("en") && m_lang != QLatin1String("pt"))
        m_lang = QStringLiteral("pt");
}

void LanguageManager::saveToSettings() const
{
    QSettings s;
    s.setValue(QStringLiteral("language"), m_lang);
}

void LanguageManager::setLang(const QString &lang)
{
    const QString next = (lang == QLatin1String("en")) ? QStringLiteral("en")
                                                       : QStringLiteral("pt");
    if (m_lang == next) return;
    m_lang = next;
    saveToSettings();
    retranslateAll();
    emit changed();
}

QString LanguageManager::tr(const QString &pt) const
{
    if (!isEnglish())
        return pt;
    const auto it = enDict().constFind(pt);
    return it == enDict().cend() ? pt : it.value();
}

void LanguageManager::bind(QObject *owner, std::function<void()> reapply)
{
    if (!owner || !reapply) return;
    Binding b;
    b.owner = owner;
    b.reapply = std::move(reapply);
    m_bindings.append(b);
    b.reapply();
    connect(owner, &QObject::destroyed, this, [this, owner]() {
        m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
            [owner](const Binding &b) { return b.owner == owner || b.owner.isNull(); }),
            m_bindings.end());
    });
}

void LanguageManager::bindText(QLabel *label, const QString &ptKey)
{
    if (!label) return;
    bind(label, [this, label, ptKey]() { label->setText(tr(ptKey)); });
}

void LanguageManager::bindText(QAbstractButton *btn, const QString &ptKey)
{
    if (!btn) return;
    bind(btn, [this, btn, ptKey]() { btn->setText(tr(ptKey)); });
}

void LanguageManager::bindPlaceholder(QLineEdit *edit, const QString &ptKey)
{
    if (!edit) return;
    bind(edit, [this, edit, ptKey]() { edit->setPlaceholderText(tr(ptKey)); });
}

void LanguageManager::bindToolTip(QWidget *w, const QString &ptKey)
{
    if (!w) return;
    bind(w, [this, w, ptKey]() { w->setToolTip(tr(ptKey)); });
}

void LanguageManager::retranslateAll()
{
    QList<Binding> live;
    live.reserve(m_bindings.size());
    for (const Binding &b : m_bindings) {
        if (b.owner.isNull()) continue;
        b.reapply();
        live.append(b);
    }
    m_bindings.swap(live);
}

} // namespace lumen::design
