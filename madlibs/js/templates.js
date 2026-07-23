/*
 * MadLibs Story Generator — Story Templates
 * -----------------------------------------
 * Each template is a story skeleton broken into labeled BEATS (Logline, Setup,
 * Inciting Incident, Conflict, Climax, Resolution) so the finished idea drops
 * straight into a storyboard / script workflow.
 *
 * Placeholder syntax (resolved by generator.js):
 *   {category}        -> a fresh random word from that category in the dictionary
 *   {category#tag}    -> a random word that is REUSED everywhere the same #tag
 *                        appears in this story, so a character's name, a place,
 *                        or a key object stays consistent across every beat.
 *   {a} / {a-cap}     -> the article "a"/"an" (or "A"/"An"), auto-chosen from
 *                        the word that immediately follows it.
 *
 * Any category name must exist in window.MADLIBS_DICT (see dictionary.js).
 *
 * To add a story: append another object to the array. Each new template
 * multiplies the total number of unique stories the app can generate.
 *
 * Exposed as: window.MADLIBS_TEMPLATES (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var TEMPLATES = [
    // ---------------------------------------------------------------- FANTASY
    {
      id: 'lost-heir',
      title: 'The Lost Heir',
      genre: 'fantasy',
      beats: [
        { label: 'Logline', text: 'A {adjective} {profession} named {name#hero} discovers they are the last heir to {place#realm}.' },
        { label: 'Setup', text: 'In the {adjective} land of {place#realm}, {name#hero} lives an unremarkable life as a {profession}, haunted by a sense of {emotion}.' },
        { label: 'Inciting Incident', text: 'During {timePeriod}, a {creature} delivers {a}{magicItem#relic} that reveals {name#hero}\'s royal bloodline.' },
        { label: 'Conflict', text: 'The {villainTitle} {name#villain}, who rules {place#realm} through fear, sends a {creature} to {verb} the true heir before the people learn the truth.' },
        { label: 'Climax', text: 'At {place}, {name#hero} confronts {name#villain}, wielding the {magicItem#relic} against {numberWord} of their soldiers.' },
        { label: 'Resolution', text: 'With {name#villain} undone, {name#hero} claims {place#realm} and rules it {adverb}, forever changed by {emotion}.' }
      ]
    },
    {
      id: 'dragon-pact',
      title: 'The Dragon\'s Pact',
      genre: 'fantasy',
      beats: [
        { label: 'Logline', text: 'To save {place#home}, a {adjective} {profession} must strike a bargain with the last {creature#beast}.' },
        { label: 'Setup', text: '{name#hero} the {profession} has spent a lifetime protecting {place#home} from the encroaching {adjective} wilds.' },
        { label: 'Inciting Incident', text: 'When {place#home} is struck by {timePeriod}, only the {creature#beast} sleeping beneath {bodyOfWater} can help.' },
        { label: 'Conflict', text: 'The {creature#beast} agrees, but demands {name#hero} {verb} the {magicItem#relic} — a price that could damn them both.' },
        { label: 'Climax', text: 'As the {creature#beast} and {name#hero} {verb} together, the {villainTitle} {name#villain} arrives to claim the {magicItem#relic} for themselves.' },
        { label: 'Resolution', text: 'The pact holds. {place#home} is saved, but {name#hero} now carries {a}{adjective} secret that binds them to the {creature#beast} {adverb}.' }
      ]
    },
    {
      id: 'witch-apprentice',
      title: 'The Reluctant Apprentice',
      genre: 'fantasy',
      beats: [
        { label: 'Logline', text: '{a-cap}{adjective} orphan is taken in by a {profession} and learns their {magicItem#relic} could unmake the world.' },
        { label: 'Setup', text: '{name#hero}, an orphan full of {emotion}, sweeps floors in the tower of {name#mentor}, a {adjective} {profession}.' },
        { label: 'Inciting Incident', text: 'One {timePeriod}, {name#hero} touches the forbidden {magicItem#relic} and {verbPast} something that should have stayed sleeping.' },
        { label: 'Conflict', text: 'Now {name#villain}, the {villainTitle}, hunts {name#hero} across {place}, certain the {magicItem#relic} belongs to them.' },
        { label: 'Climax', text: 'Cornered at {place#home}, {name#hero} must choose between saving {name#mentor} or {verbIng} the {magicItem#relic} forever.' },
        { label: 'Resolution', text: '{name#hero} masters their power {adverb}, and {place} whispers their name with {emotion}.' }
      ]
    },
    // ---------------------------------------------------------------- SCI-FI
    {
      id: 'derelict-signal',
      title: 'The Derelict Signal',
      genre: 'sci-fi',
      beats: [
        { label: 'Logline', text: 'A {adjective} {profession} aboard a {vehicle#ship} answers a distress signal that should not exist.' },
        { label: 'Setup', text: '{name#hero}, a {profession} on the {vehicle#ship} bound for {place#colony}, has been awake alone for {numberWord} cycles.' },
        { label: 'Inciting Incident', text: 'A {adjective} signal pulls them to a derelict station where a lone {object#relic} still hums with power.' },
        { label: 'Conflict', text: 'The {object#relic} holds the {pluralNoun} of a lost crew — and something aboard is {verbIng} its way toward {name#hero}.' },
        { label: 'Climax', text: 'With the {vehicle#ship} failing, {name#hero} must {verb} the {object#relic} before {name#villain}, the station\'s last survivor, can {verb} them {adverb}.' },
        { label: 'Resolution', text: '{name#hero} escapes toward {place#colony}, carrying the {object#relic} and the terrible truth of what humanity {verbPast} out there.' }
      ]
    },
    {
      id: 'memory-thief',
      title: 'The Memory Thief',
      genre: 'sci-fi',
      beats: [
        { label: 'Logline', text: 'In {place#city}, a {profession} who steals {pluralNoun} is hired to erase the one memory that could free the city.' },
        { label: 'Setup', text: '{name#hero} sells other people\'s {pluralNoun} on the black markets of {place#city}, feeling nothing but {emotion}.' },
        { label: 'Inciting Incident', text: 'The {villainTitle} {name#villain} hires {name#hero} to {verb} a single {adjective} memory from {name#target}.' },
        { label: 'Conflict', text: 'But that memory is the {object#proof} — proof that {name#villain} {verbPast} the whole city, and {name#target} is the only witness.' },
        { label: 'Climax', text: 'Inside {name#target}\'s mind, {name#hero} must {verb} {name#villain}\'s enforcers while {place#city} burns above them.' },
        { label: 'Resolution', text: '{name#hero} betrays the contract, restores the {pluralNoun}, and {place#city} remembers itself {adverb} for the first time in {numberWord} years.' }
      ]
    },
    {
      id: 'last-garden',
      title: 'The Last Garden',
      genre: 'sci-fi',
      beats: [
        { label: 'Logline', text: 'The last {profession} on a dying Earth guards {numberWord} seeds that could regrow the world — or doom it.' },
        { label: 'Setup', text: 'Under a {color} sky, {name#hero} the {profession} tends the final greenhouse in {place#ruins}.' },
        { label: 'Inciting Incident', text: 'A {vehicle} descends carrying {name#stranger}, who claims {place#ruins} must be evacuated before {timePeriod}.' },
        { label: 'Conflict', text: '{name#stranger} serves the {villainTitle} {name#villain}, who wants the seeds to {verb} a new world without the old one\'s survivors.' },
        { label: 'Climax', text: 'As {timePeriod} arrives, {name#hero} must decide whether to trust {name#stranger} or {verb} the greenhouse and everyone in it.' },
        { label: 'Resolution', text: 'One seed takes root in the {color} dust, and {name#hero} watches it grow with quiet {emotion}.' }
      ]
    },
    // ---------------------------------------------------------------- HORROR
    {
      id: 'house-that-remembers',
      title: 'The House That Remembers',
      genre: 'horror',
      beats: [
        { label: 'Logline', text: 'A grieving {profession} inherits a {adjective} house that keeps every {pluralNoun} ever spoken inside it.' },
        { label: 'Setup', text: 'After a loss that left only {emotion}, {name#hero} the {profession} moves into their late relative\'s house in {place#town}.' },
        { label: 'Inciting Incident', text: 'On the first {timePeriod}, the house replays a {adjective} conversation {name#hero} never had — in their own voice.' },
        { label: 'Conflict', text: 'The house is {verbIng} the {pluralNoun} of everyone who died within its walls, and it wants {name#hero} to stay forever.' },
        { label: 'Climax', text: 'To escape, {name#hero} must find the {noun#anchor} hidden in the walls and {verb} it before dawn.' },
        { label: 'Resolution', text: '{name#hero} flees {place#town} at first light — but sometimes, {adverb}, they still hear the house calling their name.' }
      ]
    },
    {
      id: 'thing-in-the-ice',
      title: 'The Thing in the Ice',
      genre: 'horror',
      beats: [
        { label: 'Logline', text: 'At a remote station near {bodyOfWater}, a {profession} thaws a {creature#beast} that has been waiting {numberWord} years.' },
        { label: 'Setup', text: '{name#hero} and a small crew study {bodyOfWater} from an isolated outpost, cut off by {timePeriod}.' },
        { label: 'Inciting Incident', text: 'They cut the {creature#beast} from the ice, and by morning {name#crew} is acting {adjective} — and wrong.' },
        { label: 'Conflict', text: 'The {creature#beast} is {verbIng} the crew one by one, wearing their faces, and no one can tell who is still human.' },
        { label: 'Climax', text: 'Alone with {a}{weapon}, {name#hero} must {verb} the {creature#beast} before it reaches {bodyOfWater} and the world beyond.' },
        { label: 'Resolution', text: 'The station burns behind {name#hero} on the ice, and they wait, {adverb}, to see if anything crawls out of the flames.' }
      ]
    },
    // ---------------------------------------------------------------- MYSTERY
    {
      id: 'clockmaker-murder',
      title: 'The Clockmaker\'s Last Hour',
      genre: 'mystery',
      beats: [
        { label: 'Logline', text: 'A {adjective} {profession} must solve a murder where every clue is a stopped clock.' },
        { label: 'Setup', text: 'In {place#city}, {name#hero} the {profession} is called to the death of {name#victim}, a famous clockmaker.' },
        { label: 'Inciting Incident', text: 'Every clock in the house stopped at the same minute — and one of them hides {a}{object#clue}.' },
        { label: 'Conflict', text: 'The trail leads to the {villainTitle} {name#villain}, but the only witness, {name#witness}, has vanished into {place}.' },
        { label: 'Climax', text: 'At {place}, {name#hero} confronts {name#villain} and reveals how the {object#clue} proves they {verbPast} {name#victim} {adverb}.' },
        { label: 'Resolution', text: 'Justice done, {name#hero} winds the dead clockmaker\'s last {noun} and lets it tick again, filled with {emotion}.' }
      ]
    },
    {
      id: 'missing-painting',
      title: 'The Vanishing Canvas',
      genre: 'mystery',
      beats: [
        { label: 'Logline', text: 'A disgraced {profession} chases a stolen {noun#art} that seems to change what it shows.' },
        { label: 'Setup', text: '{name#hero}, once a celebrated {profession}, now works the forgotten cases in {place#city}.' },
        { label: 'Inciting Incident', text: 'A priceless {noun#art} is stolen from {place#gallery}, and the only clue is a {color} thread.' },
        { label: 'Conflict', text: 'Each person who owned the {noun#art} met a {adjective} end, and {name#villain} the {villainTitle} will kill to own it next.' },
        { label: 'Climax', text: 'In {place#gallery} after dark, {name#hero} must {verb} {name#villain} and expose what the {noun#art} truly {verbPast}.' },
        { label: 'Resolution', text: 'The {noun#art} is returned, but {name#hero} swears it winked at them — and walks away, {adverb} unsettled.' }
      ]
    },
    // ---------------------------------------------------------------- ROMANCE
    {
      id: 'rival-bakers',
      title: 'Rivals at the Harvest',
      genre: 'romance',
      beats: [
        { label: 'Logline', text: 'Two rival {profession}s must share one kitchen to win the {place#town} {timePeriod} contest.' },
        { label: 'Setup', text: '{name#hero}, a {adjective} {profession}, has won the {place#town} contest {numberWord} years running — until {name#love} arrives.' },
        { label: 'Inciting Incident', text: 'A fire leaves them {verbIng} for the same prize in the same kitchen, tempers hot as the ovens.' },
        { label: 'Conflict', text: 'Between arguments over {food} and {food}, {name#hero} and {name#love} feel an unwelcome {emotion} they both deny.' },
        { label: 'Climax', text: 'On the eve of {timePeriod}, one must sabotage the other\'s dish — or confess the truth {adverb}.' },
        { label: 'Resolution', text: 'They lose the contest together and win something better, sharing {a}{food} beneath the {color} sky.' }
      ]
    },
    {
      id: 'letters-lighthouse',
      title: 'Letters from the Lighthouse',
      genre: 'romance',
      beats: [
        { label: 'Logline', text: 'A lonely {profession} falls for a stranger they only know through {pluralNoun} washed up from {bodyOfWater}.' },
        { label: 'Setup', text: '{name#hero} keeps the lighthouse above {bodyOfWater} near {place#town}, with only {emotion} for company.' },
        { label: 'Inciting Incident', text: 'A {noun} drifts ashore holding a letter from {name#love}, stranded somewhere out on {bodyOfWater}.' },
        { label: 'Conflict', text: 'For {numberWord} weeks they trade letters by tide, but {timePeriod} is coming, and {name#love} is running out of time.' },
        { label: 'Climax', text: 'When the {timePeriod} storm hits, {name#hero} takes {a}{vehicle} into {bodyOfWater} to {verb} the stranger they\'ve never met.' },
        { label: 'Resolution', text: 'Two strangers meet on the shore at last, {adverb}, the lighthouse burning warm behind them.' }
      ]
    },
    // ---------------------------------------------------------------- COMEDY
    {
      id: 'accidental-hero',
      title: 'The Accidental Hero',
      genre: 'comedy',
      beats: [
        { label: 'Logline', text: 'A clumsy {profession} is mistaken for {a}legendary {creature#beast}-slayer and has to keep the lie alive.' },
        { label: 'Setup', text: '{name#hero}, the most {adjective} {profession} in {place#town}, has never {verbPast} anything more dangerous than {a}{food}.' },
        { label: 'Inciting Incident', text: 'After tripping into the {creature#beast} and knocking it out by pure luck, the whole town declares {name#hero} a legend.' },
        { label: 'Conflict', text: 'Now the {villainTitle} {name#villain} demands {name#hero} slay a real {creature#beast}, and the truth would ruin everything.' },
        { label: 'Climax', text: 'Armed only with {a}{noun} and sheer panic, {name#hero} faces the {creature#beast} while the whole of {place#town} watches.' },
        { label: 'Resolution', text: 'Through {numberWord} ridiculous accidents, {name#hero} wins anyway, and accepts the applause {adverb}.' }
      ]
    },
    {
      id: 'wedding-disaster',
      title: 'The Wedding of the Century',
      genre: 'comedy',
      beats: [
        { label: 'Logline', text: 'A frazzled {profession} has one {timePeriod} to save a wedding after the {noun#item}, the {food}, and the {animal#critter} all go missing.' },
        { label: 'Setup', text: '{name#hero} the {profession} has planned the {place#town} wedding of {name#bride} down to the last {noun#item}.' },
        { label: 'Inciting Incident', text: 'The morning of, the {food} is ruined, the {noun#item} is gone, and someone released {a}{animal#critter} into the hall.' },
        { label: 'Conflict', text: 'Worse, the {villainTitle} {name#villain} — the bride\'s ex — is {verbIng} to ruin everything {adverb}.' },
        { label: 'Climax', text: 'With minutes left, {name#hero} must wrangle the {animal#critter}, replace the {food}, and out-scheme {name#villain} at once.' },
        { label: 'Resolution', text: 'Somehow the wedding is perfect, the {animal#critter} becomes the guest of honor, and {name#hero} finally feels {emotion}.' }
      ]
    },
    // ---------------------------------------------------------------- ADVENTURE
    {
      id: 'map-of-the-dead',
      title: 'The Map of the Dead',
      genre: 'adventure',
      beats: [
        { label: 'Logline', text: 'A treasure-hunting {profession} follows {a}{magicItem#relic} toward a fortune buried beneath {bodyOfWater}.' },
        { label: 'Setup', text: '{name#hero}, {a}{adjective} {profession}, has chased legends across {place#region} for {numberWord} years and found only {emotion}.' },
        { label: 'Inciting Incident', text: '{a-cap}dying sailor presses the {magicItem#relic} into {name#hero}\'s {bodyPart}, whispering of gold beneath {bodyOfWater}.' },
        { label: 'Conflict', text: 'The {villainTitle} {name#villain} wants the same fortune, and the path runs straight through {a}{creature#beast}\'s lair.' },
        { label: 'Climax', text: 'At the bottom of {bodyOfWater}, {name#hero} must {verb} {name#villain} and the {creature#beast} to reach the {object} at last.' },
        { label: 'Resolution', text: 'Richer in scars than gold, {name#hero} sails home {adverb}, already dreaming of {place#region}\'s next secret.' }
      ]
    },
    {
      id: 'sky-pirates',
      title: 'Sky Pirates of the Amber Isles',
      genre: 'adventure',
      beats: [
        { label: 'Logline', text: 'A runaway {profession} joins the crew of {a}{vehicle#ship} to steal the {object#relic} that powers a floating empire.' },
        { label: 'Setup', text: '{name#hero} flees a {adjective} life to crew the {vehicle#ship} captained by the notorious {name#captain}.' },
        { label: 'Inciting Incident', text: 'The crew learns the {villainTitle} {name#villain} guards {a}{object#relic} that keeps {place#empire} afloat.' },
        { label: 'Conflict', text: 'To steal it, {name#hero} must {verb} past {numberWord} sky-guards while a mutiny brews aboard the {vehicle#ship}.' },
        { label: 'Climax', text: 'As {place#empire} tilts from the sky, {name#hero} and {name#captain} duel {name#villain} on the deck of the {vehicle#ship}.' },
        { label: 'Resolution', text: 'The {object#relic} is won, {place#empire} falls gently to earth, and {name#hero} finally belongs somewhere, {adverb} free.' }
      ]
    },
    // ---------------------------------------------------------------- THRILLER
    {
      id: 'wrong-suitcase',
      title: 'The Wrong Suitcase',
      genre: 'thriller',
      beats: [
        { label: 'Logline', text: 'An ordinary {profession} grabs the wrong {noun#bag} at {place#station} and inherits a killer\'s {pluralNoun}.' },
        { label: 'Setup', text: '{name#hero}, {a}{adjective} {profession}, is just trying to get home through {place#station}.' },
        { label: 'Inciting Incident', text: 'A swapped {noun#bag} leaves {name#hero} holding {a}{object#relic} that {numberWord} dangerous people want back.' },
        { label: 'Conflict', text: 'The {villainTitle} {name#villain} is {verbIng} {name#hero} across {place#city}, and the police think {name#hero} is the criminal.' },
        { label: 'Climax', text: 'Trapped at {place}, {name#hero} must expose what the {object#relic} really contains before {name#villain} can {verb} them {adverb}.' },
        { label: 'Resolution', text: 'The truth breaks wide open, {name#villain} falls, and {name#hero} goes home changed — always checking over one {bodyPart}.' }
      ]
    },
    {
      id: 'countdown',
      title: 'The Countdown',
      genre: 'thriller',
      beats: [
        { label: 'Logline', text: 'A {profession} has {numberWord} hours to find {a}{object#bomb} hidden somewhere in {place#city} before it goes off.' },
        { label: 'Setup', text: '{name#hero} the {profession} receives a {adjective} call: an anonymous voice, and a clock already {verbIng} down.' },
        { label: 'Inciting Incident', text: 'The voice — the {villainTitle} {name#villain} — has hidden the {object#bomb} and dares {name#hero} to find it.' },
        { label: 'Conflict', text: 'Each clue costs a life, and {name#villain} always seems to know where {name#hero} will look next.' },
        { label: 'Climax', text: 'With the clock near zero at {place}, {name#hero} realizes {name#villain} is someone they {verbPast} to trust.' },
        { label: 'Resolution', text: 'The {object#bomb} is stopped with seconds to spare, and {name#hero} sits in the silence, {adverb} hollow with {emotion}.' }
      ]
    },
    // ---------------------------------------------------------------- HEIST
    {
      id: 'one-last-job',
      title: 'One Last Job',
      genre: 'heist',
      beats: [
        { label: 'Logline', text: 'A retired {profession} assembles a crew to steal {a}{object#relic} from the {villainTitle} who ruined them.' },
        { label: 'Setup', text: '{name#hero}, the best {profession} in {place#city}, walked away from the game after one {adjective} betrayal.' },
        { label: 'Inciting Incident', text: 'The {villainTitle} {name#villain} resurfaces holding the {object#relic} that {name#hero} would risk everything to take back.' },
        { label: 'Conflict', text: 'The crew must {verb} past {numberWord} guards, but someone inside is quietly {verbIng} for {name#villain}.' },
        { label: 'Climax', text: 'Mid-heist at {place#vault}, the double-cross snaps shut, and {name#hero} must improvise to grab the {object#relic} and escape.' },
        { label: 'Resolution', text: 'The score is settled, the crew scatters {adverb}, and {name#hero} finally lets go of the {emotion} they carried for years.' }
      ]
    },
    {
      id: 'museum-ghost',
      title: 'The Museum Ghost',
      genre: 'heist',
      beats: [
        { label: 'Logline', text: 'A master thief poses as a {profession} to lift {a}{magicItem#prize} from the most guarded museum in {place#city}.' },
        { label: 'Setup', text: '{name#hero} has never been caught, slipping through {place#city} like one of its {pluralNoun}.' },
        { label: 'Inciting Incident', text: 'A mysterious client offers a fortune for the {magicItem#prize} displayed under {numberWord} layers of security.' },
        { label: 'Conflict', text: 'The museum\'s new head of security, {name#rival}, is {adverb} brilliant — and seems to anticipate {name#hero}\'s every move.' },
        { label: 'Climax', text: 'On the night of {timePeriod}, {name#hero} and {name#rival} play a silent game of wits around the {magicItem#prize}.' },
        { label: 'Resolution', text: 'The {magicItem#prize} vanishes, {name#rival} smiles at an empty case, and {name#hero} disappears into {place#city}, {adverb} satisfied.' }
      ]
    },
    // ---------------------------------------------------------------- WESTERN
    {
      id: 'last-train',
      title: 'The Last Train to Dredmoor',
      genre: 'western',
      beats: [
        { label: 'Logline', text: 'A weary {profession} boards the last {vehicle#train} to {place#town} to settle {a}{adjective} debt of blood.' },
        { label: 'Setup', text: '{name#hero}, {a}{adjective} {profession} with nothing left but {emotion}, rides toward {place#town}.' },
        { label: 'Inciting Incident', text: 'Word comes that the {villainTitle} {name#villain} — who {verbPast} {name#hero}\'s family — waits at the end of the line.' },
        { label: 'Conflict', text: 'The {vehicle#train} is full of {name#villain}\'s hired guns, and {numberWord} miles of track lie between {name#hero} and vengeance.' },
        { label: 'Climax', text: 'In the streets of {place#town} at high noon, {name#hero} and {name#villain} finally {verb} it out with {weapon} in hand.' },
        { label: 'Resolution', text: 'The dust settles, the debt is paid, and {name#hero} rides out of {place#town} {adverb}, carrying only {emotion}.' }
      ]
    },
    // ---------------------------------------------------------------- SUPERHERO
    {
      id: 'reluctant-mask',
      title: 'The Reluctant Mask',
      genre: 'superhero',
      beats: [
        { label: 'Logline', text: 'A cynical {profession} gains the power to {verb} {pluralNoun} and must protect the city that fears them.' },
        { label: 'Setup', text: '{name#hero} the {profession} scrapes by in {place#city}, wanting nothing to do with heroes or their {emotion}.' },
        { label: 'Inciting Incident', text: 'After a run-in with {a}{object}, {name#hero} wakes able to {verb} {pluralNoun} with a thought.' },
        { label: 'Conflict', text: 'The {villainTitle} {name#villain} plans to {verb} {place#city}, and only {name#hero} can stop them — if they choose to.' },
        { label: 'Climax', text: 'As {place#city} burns, {name#hero} dons the mask and faces {name#villain} above the {adjective} skyline.' },
        { label: 'Resolution', text: '{place#city} has a new guardian, and {name#hero} watches over it {adverb}, still pretending they don\'t care.' }
      ]
    },
    // ---------------------------------------------------------------- NOIR
    {
      id: 'dame-in-red',
      title: 'The Dame in Red',
      genre: 'noir',
      beats: [
        { label: 'Logline', text: '{a-cap}down-on-their-luck {profession} takes one case from a {color#hue} stranger and falls into a city of {pluralNoun}.' },
        { label: 'Setup', text: 'Rain never stops in {place#city}, and neither does the {emotion} that follows {name#hero}, a {adjective} {profession}.' },
        { label: 'Inciting Incident', text: '{name#client}, dressed in {color#hue}, walks in and asks {name#hero} to find {a}missing {noun#item}.' },
        { label: 'Conflict', text: 'The {noun#item} ties back to the {villainTitle} {name#villain}, and everyone who touches the case ends up {verbPast}.' },
        { label: 'Climax', text: 'In a {adjective} room above {place}, {name#hero} learns {name#client} has been {verbIng} them all along.' },
        { label: 'Resolution', text: 'The case closes cold. {name#hero} pours {a}{food}, watches the {color#hue} neon flicker, and lets the {emotion} settle {adverb}.' }
      ]
    },
    // ---------------------------------------------------------------- FAIRY TALE
    {
      id: 'miller-daughter',
      title: 'The Miller\'s Bargain',
      genre: 'fairy-tale',
      beats: [
        { label: 'Logline', text: 'A poor {profession}\'s child promises {a}{creature#beast} their firstborn in exchange for {a}{magicItem#relic}.' },
        { label: 'Setup', text: 'In the village of {place#town}, {name#hero}, child of a humble {profession}, dreams beyond their {adjective} lot.' },
        { label: 'Inciting Incident', text: 'A cunning {creature#beast} appears offering {a}{magicItem#relic}, if only {name#hero} will pay {a}{adjective} price.' },
        { label: 'Conflict', text: 'The {magicItem#relic} brings fortune, but the {creature#beast} returns at {timePeriod} to collect what it is owed.' },
        { label: 'Climax', text: 'To break the bargain, {name#hero} must guess the {creature#beast}\'s true name before the {timePeriod} bell tolls.' },
        { label: 'Resolution', text: 'The name is spoken, the {creature#beast} vanishes {adverb}, and {place#town} tells the tale for {numberWord} generations.' }
      ]
    },
    // ---------------------------------------------------------------- POST-APOCALYPTIC
    {
      id: 'long-walk-home',
      title: 'The Long Walk Home',
      genre: 'post-apocalyptic',
      beats: [
        { label: 'Logline', text: 'Across a ruined {place#region}, a {profession} escorts {a}{adjective} child rumored to be the world\'s cure.' },
        { label: 'Setup', text: 'Years after the collapse, {name#hero} survives alone in the wastes of {place#region}, trading in {pluralNoun}.' },
        { label: 'Inciting Incident', text: 'A dying settlement begs {name#hero} to carry {name#child} across {place#region} to the last standing outpost.' },
        { label: 'Conflict', text: 'The {villainTitle} {name#villain} and their raiders are {verbIng} them, certain {name#child} is worth more dead.' },
        { label: 'Climax', text: 'At the gates of the outpost, {name#hero} makes a last stand against {name#villain} with only {a}{weapon} and {emotion}.' },
        { label: 'Resolution', text: '{name#child} passes safely through the gate, and {name#hero} walks back into the {adjective} wastes, {adverb} at peace.' }
      ]
    },
    // ---------------------------------------------------------------- MYTH / EPIC
    {
      id: 'stolen-fire',
      title: 'The Theft of Fire',
      genre: 'mythic',
      beats: [
        { label: 'Logline', text: 'A mortal {profession} climbs to the realm of the gods to {verb} {a}{magicItem#relic} for their dying people.' },
        { label: 'Setup', text: '{place#land} is dying under {timePeriod}, and only {name#hero}, {a}{adjective} {profession}, dares to act.' },
        { label: 'Inciting Incident', text: 'A {creature} reveals that the gods hoard {a}{magicItem#relic} that could save {place#land}.' },
        { label: 'Conflict', text: 'To take it, {name#hero} must {verb} past {numberWord} trials and the wrath of the {villainTitle} {name#villain}.' },
        { label: 'Climax', text: 'At the summit, {name#hero} seizes the {magicItem#relic} as {name#villain} vows to make them suffer for {numberWord} lifetimes.' },
        { label: 'Resolution', text: '{place#land} is saved, but {name#hero} is bound to a {adjective} punishment, remembered {adverb} as both thief and savior.' }
      ]
    },
    // ---------------------------------------------------------------- SPY
    {
      id: 'double-agent',
      title: 'The Double Agent',
      genre: 'spy',
      beats: [
        { label: 'Logline', text: 'A {profession} deep undercover must decide whether to {verb} the {villainTitle} they\'ve grown to admire.' },
        { label: 'Setup', text: '{name#hero} has spent {numberWord} years posing as a loyal aide inside the court of {place#state}.' },
        { label: 'Inciting Incident', text: 'Their handlers order {name#hero} to steal the {object#secret} and {verb} the {villainTitle} {name#villain}.' },
        { label: 'Conflict', text: 'But {name#villain} has treated {name#hero} with a {adjective} kindness that makes betrayal taste like {emotion}.' },
        { label: 'Climax', text: 'At the {timePeriod} gala, {name#hero} must choose in an instant: seize the {object#secret}, or warn {name#villain} {adverb}.' },
        { label: 'Resolution', text: 'Whatever {name#hero} chooses, they leave {place#state} a different person, haunted by {emotion}.' }
      ]
    },
    // ---------------------------------------------------------------- COMING OF AGE
    {
      id: 'summer-of-crows',
      title: 'The Summer of Crows',
      genre: 'coming-of-age',
      beats: [
        { label: 'Logline', text: 'One restless summer in {place#town}, a {adjective} kid and their friends chase a local legend about {a}{creature#beast}.' },
        { label: 'Setup', text: '{name#hero} is stuck in sleepy {place#town} with nothing but {emotion} and {numberWord} bored friends.' },
        { label: 'Inciting Incident', text: 'They find {a}{noun#clue} in the woods tied to the old story of the {creature#beast} near {bodyOfWater}.' },
        { label: 'Conflict', text: 'Chasing the truth pits them against {name#villain} and forces {name#hero} to face a {adjective} family secret.' },
        { label: 'Climax', text: 'At {bodyOfWater} on the last night of summer, {name#hero} finally learns what the {noun#clue} — and the legend — really mean.' },
        { label: 'Resolution', text: 'Summer ends, the friends drift {adverb} apart, but {name#hero} carries that summer forever, changed by it.' }
      ]
    },
    // ---------------------------------------------------------------- DISASTER
    {
      id: 'when-the-water-came',
      title: 'When the Water Came',
      genre: 'disaster',
      beats: [
        { label: 'Logline', text: 'As {bodyOfWater} rises to swallow {place#city}, a {profession} races to save {numberWord} strangers and one they love.' },
        { label: 'Setup', text: '{name#hero} the {profession} knows {place#city} better than anyone — every street, every {noun}.' },
        { label: 'Inciting Incident', text: 'During {timePeriod}, {bodyOfWater} breaches its banks and begins {verbIng} the lower city block by block.' },
        { label: 'Conflict', text: 'With rescue cut off and {name#loved} trapped across the flood, {name#hero} must {verb} through the rising dark.' },
        { label: 'Climax', text: 'On the last dry rooftop, {name#hero} must choose between {numberWord} strangers and reaching {name#loved} in time.' },
        { label: 'Resolution', text: 'The waters recede from {place#city} at last, and the survivors rebuild {adverb}, bound now by what they {verbPast}.' }
      ]
    },
    // ---------------------------------------------------------------- POLITICAL
    {
      id: 'the-vote',
      title: 'The Vote',
      genre: 'political',
      beats: [
        { label: 'Logline', text: 'An idealistic {profession} uncovers that the {villainTitle} ruling {place#state} has been {verbIng} the people for years.' },
        { label: 'Setup', text: '{name#hero}, {a}{adjective} {profession}, believes in {place#state} and the promises of the {villainTitle} {name#villain}.' },
        { label: 'Inciting Incident', text: 'A leaked {object#leak} reveals {name#villain} has {verbPast} {place#state} to keep power.' },
        { label: 'Conflict', text: 'Exposing the truth means risking everything, and {name#villain} controls {numberWord} people willing to bury {name#hero}.' },
        { label: 'Climax', text: 'On the eve of the vote, {name#hero} brings the {object#leak} before the whole of {place#state}.' },
        { label: 'Resolution', text: '{place#state} chooses its future, and {name#hero} learns that {emotion}, not power, is what truly holds a nation together.' }
      ]
    },
    // ---------------------------------------------------------------- STEAMPUNK
    {
      id: 'clockwork-rebellion',
      title: 'The Clockwork Rebellion',
      genre: 'steampunk',
      beats: [
        { label: 'Logline', text: 'A {profession} in the smoke-choked city of {place#city} builds {a}{object#invention} that could free its {pluralNoun}.' },
        { label: 'Setup', text: '{name#hero} the {profession} tinkers by candlelight beneath the gears of {place#city}, dreaming of a {adjective} dawn.' },
        { label: 'Inciting Incident', text: 'When {name#hero} completes the {object#invention}, it awakens {a}{creature#beast} of brass that speaks of revolution.' },
        { label: 'Conflict', text: 'The {villainTitle} {name#villain} who owns {place#city}\'s factories will {verb} anyone who threatens the great machine.' },
        { label: 'Climax', text: 'Atop the central gearworks, {name#hero} and the brass {creature#beast} must {verb} {name#villain} before {timePeriod}.' },
        { label: 'Resolution', text: 'The gears of {place#city} turn to a new rhythm, and its {pluralNoun} breathe free air {adverb} for the first time.' }
      ]
    },
    // ---------------------------------------------------------------- SURVIVAL
    {
      id: 'against-the-storm',
      title: 'Against the Storm',
      genre: 'survival',
      beats: [
        { label: 'Logline', text: 'Stranded near {bodyOfWater} after a {vehicle#wreck} wreck, a {profession} must survive {timePeriod} with a wounded stranger.' },
        { label: 'Setup', text: '{name#hero} the {profession} is the sole survivor of a {vehicle#wreck} that went down near {bodyOfWater} — almost.' },
        { label: 'Inciting Incident', text: 'They find {name#stranger}, badly hurt, and {timePeriod} closing in with nothing but {a}{noun} between them and death.' },
        { label: 'Conflict', text: 'Food runs out, {name#stranger} fades, and {a}{creature} circles their {adjective} shelter each night.' },
        { label: 'Climax', text: 'To reach rescue, {name#hero} must cross {bodyOfWater} carrying {name#stranger}, {verbIng} through the worst of the storm.' },
        { label: 'Resolution', text: 'They wash ashore alive, forever bound by the {timePeriod} they {verbPast} together, {adverb} grateful.' }
      ]
    }
  ];

  root.MADLIBS_TEMPLATES = TEMPLATES;
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = TEMPLATES;
  }
})(typeof window !== 'undefined' ? window : this);
