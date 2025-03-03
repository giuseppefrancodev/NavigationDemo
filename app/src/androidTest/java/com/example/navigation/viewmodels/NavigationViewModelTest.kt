import androidx.test.ext.junit.runners.AndroidJUnit4
import com.example.navigation.FakeNavigationEngine
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.repository.FakeNavigationRepository
import com.example.navigation.viewmodels.NavigationViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class NavigationViewModelTest {

    private lateinit var fakeRepository: FakeNavigationRepository
    private lateinit var fakeNavigationEngine: FakeNavigationEngine
    private lateinit var viewModel: NavigationViewModel

    private val testDispatcher = UnconfinedTestDispatcher()

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)

        fakeRepository = FakeNavigationRepository()
        fakeNavigationEngine = FakeNavigationEngine()

        viewModel = NavigationViewModel(fakeRepository, fakeNavigationEngine)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun testSetDestinationUpdatesState() = runTest {
        // Arrange
        val latitude = 60.16
        val longitude = 24.93
        val testRoute = Route(
            id = "test-route",
            points = listOf(
                Location(60.16, 24.93),
                Location(60.17, 24.94)
            ),
            durationSeconds = 300,
            name = "Test Route"
        )

        // Configure the fake
        fakeNavigationEngine.setDestinationSuccess(true)
        fakeNavigationEngine.setAlternativeRoutes(listOf(testRoute))

        // Act
        viewModel.setDestination(latitude, longitude)

        // Assert
        val state = viewModel.state.value
        assertNotNull(state.destination)
        assertEquals(latitude, state.destination?.latitude)
        assertEquals(longitude, state.destination?.longitude)
        assertEquals(testRoute.id, state.alternativeRoutes[0].id)
    }

    @Test
    fun testStartNavigationUpdatesState() = runTest {
        // Arrange
        val testRoute = Route(
            id = "test-route",
            points = listOf(Location(60.16, 24.93)),
            durationSeconds = 100,
            name = "Test Route"
        )

        fakeNavigationEngine.setDestinationSuccess(true)
        fakeNavigationEngine.setAlternativeRoutes(listOf(testRoute))
        viewModel.setDestination(60.17, 24.94)

        // Act
        viewModel.startNavigation()

        // Assert
        val state = viewModel.state.value
        assertTrue(state.isNavigating)
    }

    @Test
    fun testStopNavigationUpdatesState() = runTest {
        // Arrange
        val testRoute = Route(
            id = "test-route",
            points = listOf(Location(60.16, 24.93)),
            durationSeconds = 100,
            name = "Test Route"
        )
        fakeNavigationEngine.setDestinationSuccess(true)
        fakeNavigationEngine.setAlternativeRoutes(listOf(testRoute))

        // Set destination, start navigation
        viewModel.setDestination(60.17, 24.94)
        viewModel.startNavigation()

        // Act
        viewModel.stopNavigation()

        // Assert
        val state = viewModel.state.value
        assertFalse(state.isNavigating)
    }

    @Test
    fun testClearDestinationResetsState() = runTest {
        // Arrange
        val testRoute = Route(
            id = "test-route",
            points = listOf(Location(60.16, 24.93)),
            durationSeconds = 100,
            name = "Test Route"
        )
        fakeNavigationEngine.setDestinationSuccess(true)
        fakeNavigationEngine.setAlternativeRoutes(listOf(testRoute))

        // Set destination, start navigation
        viewModel.setDestination(60.17, 24.94)
        viewModel.startNavigation()

        // Act
        viewModel.clearDestination()

        // Assert
        val state = viewModel.state.value
        assertNull(state.destination)
        assertFalse(state.isNavigating)
        assertTrue(state.alternativeRoutes.isEmpty())
    }

    @Test
    fun testLocationPermissionGrantedUpdatesLocation() = runTest {
        // Arrange
        val testLocation = Location(60.16, 24.93, 0f, 0f)

        // Grant permission so the ViewModel starts collecting from the fake repository
        viewModel.onLocationPermissionGranted(true)

        // Act: emit a location from the fake
        fakeRepository.emitLocation(testLocation)

        // Let coroutines settle
        advanceUntilIdle()

        // Assert
        val state = viewModel.state.value
        assertEquals(testLocation.latitude, state.currentLocation?.latitude)
        assertEquals(testLocation.longitude, state.currentLocation?.longitude)
    }

    @Test
    fun testSetErrorUpdatesErrorMessage() = runTest {
        // Arrange
        val errorMessage = "Test error"

        // Act
        viewModel.setError(errorMessage)

        // Assert
        val state = viewModel.state.value
        assertEquals(errorMessage, state.errorMessage)
    }

    @Test
    fun testSelectRouteUpdatesCurrentRoute() = runTest {
        // Arrange
        val testRoute1 = Route(
            id = "test-route-1",
            points = listOf(Location(60.16, 24.93)),
            durationSeconds = 100,
            name = "Test Route 1"
        )
        val testRoute2 = Route(
            id = "test-route-2",
            points = listOf(Location(60.17, 24.94)),
            durationSeconds = 200,
            name = "Test Route 2"
        )

        fakeNavigationEngine.setDestinationSuccess(true)
        fakeNavigationEngine.setAlternativeRoutes(listOf(testRoute1, testRoute2))

        // Set the destination so we have alternative routes in state
        viewModel.setDestination(60.16, 24.93)

        // Act
        viewModel.selectRoute("test-route-2")

        // Assert
        val state = viewModel.state.value
        assertEquals("test-route-2", state.currentRoute?.id)
    }
}
